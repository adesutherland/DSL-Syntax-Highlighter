#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif
#include <errno.h>

#include "dslsyntax_common.h"
#include "dslsyntax_parser.h"
#include "serialization.h"
#include "dslsyntax_log.h"

#ifdef _WIN32
#define CLOSE_SOCKET closesocket
typedef SOCKET SocketHandle;
#define INVALID_SOCKET_HANDLE INVALID_SOCKET
#else
#define CLOSE_SOCKET close
typedef int SocketHandle;
#define INVALID_SOCKET_HANDLE (-1)
#endif

typedef struct ParserDocumentSession {
    char *document_id;
    CodeBuffer *code_buffer;
    struct ParserDocumentSession *next;
} ParserDocumentSession;

static CodeBuffer *create_parser_code_buffer_like(CodeBuffer *template_cb) {
    return create_code_buffer(NULL, template_cb ? template_cb->parser_function : NULL);
}

static ParserDocumentSession *find_parser_session(ParserDocumentSession *sessions, const char *document_id) {
    while (sessions) {
        if (sessions->document_id && document_id && strcmp(sessions->document_id, document_id) == 0) {
            return sessions;
        }
        sessions = sessions->next;
    }
    return NULL;
}

static ParserDocumentSession *single_parser_session(ParserDocumentSession *sessions) {
    if (!sessions || sessions->next) return NULL;
    return sessions;
}

static ParserDocumentSession *replace_parser_session(ParserDocumentSession **sessions,
                                                     CodeBuffer *template_cb,
                                                     const char *document_id) {
    ParserDocumentSession *session;

    if (!sessions || !document_id) return NULL;
    session = find_parser_session(*sessions, document_id);
    if (!session) {
        session = (ParserDocumentSession*)calloc(1, sizeof(ParserDocumentSession));
        if (!session) return NULL;
        session->document_id = strdup(document_id);
        session->next = *sessions;
        *sessions = session;
    } else if (session->code_buffer) {
        free_code_buffer(session->code_buffer);
        session->code_buffer = NULL;
    }
    session->code_buffer = create_parser_code_buffer_like(template_cb);
    return session;
}

static void free_parser_sessions(ParserDocumentSession *sessions) {
    while (sessions) {
        ParserDocumentSession *next = sessions->next;
        if (sessions->document_id) free(sessions->document_id);
        if (sessions->code_buffer) free_code_buffer(sessions->code_buffer);
        free(sessions);
        sessions = next;
    }
}

static InitialLoad *create_initial_load_from_source(const char *document_id,
                                                    const char *source,
                                                    size_t change_version) {
    InitialLoad *load;
    size_t length;
    size_t start;

    load = (InitialLoad*)calloc(1, sizeof(InitialLoad));
    if (!load) return NULL;
    load->unique_document_id = strdup(document_id ? document_id : "hypothesis");
    load->change_version = change_version;
    if (!load->unique_document_id) {
        free(load);
        return NULL;
    }

    source = source ? source : "";
    length = strlen(source);
    start = 0;
    for (size_t i = 0; i <= length; i++) {
        if (source[i] == '\0' || source[i] == '\n') {
            if (source[i] == '\0' && i == start && i > 0 && source[i - 1] == '\n') {
                break;
            }
            size_t line_len = i - start;
            char *line = (char*)malloc(line_len + 1);
            CodeBufferLine *new_lines;
            if (!line) {
                free_initial_load(load);
                return NULL;
            }
            memcpy(line, source + start, line_len);
            line[line_len] = '\0';
            new_lines = (CodeBufferLine*)safe_realloc(load->lines, (load->line_count + 1) * sizeof(CodeBufferLine));
            if (!new_lines) {
                free(line);
                free_initial_load(load);
                return NULL;
            }
            load->lines = new_lines;
            utf8_to_line(line, &load->lines[load->line_count++]);
            free(line);
            start = i + 1;
        }
    }
    return load;
}

static CB_ParseTree *parse_hypothesis_on_copy(CodeBuffer *source_cb, Delta *delta) {
    CodeBuffer *scratch;
    InitialLoad *load;
    char *source;
    CB_ParseTree *result;

    if (!source_cb || !delta) return NULL;
    if (delta->base_version != source_cb->change_version) {
        LOG("Hypothesis version mismatch: document=%s base=%zu current=%zu",
            source_cb->unique_document_id ? source_cb->unique_document_id : "",
            delta->base_version,
            source_cb->change_version);
        return NULL;
    }

    source = get_code_buffer_source(source_cb);
    if (!source) return NULL;

    scratch = create_parser_code_buffer_like(source_cb);
    if (!scratch) {
        free(source);
        return NULL;
    }

    load = create_initial_load_from_source(source_cb->unique_document_id ? source_cb->unique_document_id : "hypothesis",
                                           source,
                                           source_cb->change_version);
    free(source);
    if (!load) {
        free_code_buffer(scratch);
        return NULL;
    }
    base_load_initial_content(scratch, load);
    base_replay_delta(scratch, delta);
    base_parse_buffer(scratch);

    result = scratch->parse_tree;
    scratch->parse_tree = NULL;
    free_code_buffer(scratch);
    return result;
}

static CB_ParseTree *handle_parser_request(CodeBuffer *legacy_cb,
                                           ParserDocumentSession **sessions,
                                           char *req) {
    CodeBuffer *target_cb;
    ParserDocumentSession *session;
    CB_ParseTree *hypothesis_result;

    if (!legacy_cb || !req) return NULL;

    if (req[0] == 'I') {
        InitialLoad *load = cb_deserialize_initial_load(req + 2);
        if (!load) return NULL;
        if (load->unique_document_id) {
            session = replace_parser_session(sessions, legacy_cb, load->unique_document_id);
            if (!session || !session->code_buffer) {
                free_initial_load(load);
                return NULL;
            }
            base_load_initial_content(session->code_buffer, load);
            return session->code_buffer->parse_tree;
        }
        base_load_initial_content(legacy_cb, load);
        return legacy_cb->parse_tree;
    }

    if (req[0] == 'D' || req[0] == 'H') {
        Delta *delta = cb_deserialize_delta(req + 2);
        if (!delta) return NULL;
        target_cb = legacy_cb;
        if (delta->unique_document_id) {
            session = find_parser_session(sessions ? *sessions : NULL, delta->unique_document_id);
            if (!session || !session->code_buffer) {
                LOG("No parser session for document %s", delta->unique_document_id);
                free_delta(delta);
                return NULL;
            }
            target_cb = session->code_buffer;
        } else {
            session = single_parser_session(sessions ? *sessions : NULL);
            if (session && session->code_buffer) {
                target_cb = session->code_buffer;
            }
        }

        if (req[0] == 'H') {
            hypothesis_result = parse_hypothesis_on_copy(target_cb, delta);
            free_delta(delta);
            return hypothesis_result;
        }

        base_replay_delta(target_cb, delta);
        base_parse_buffer(target_cb);
        free_delta(delta);
        return target_cb->parse_tree;
    }

    return legacy_cb->parse_tree;
}

/* --- In-Process Comms --- */

typedef struct {
    CommunicationFunctions *comm;
    CodeBuffer *parser_code_buffer;
} InprocCommsData;

static CB_ParseTree * inproc_send_initial_load(CommunicationFunctions *comm_block, InitialLoad *initial_load) {
    InprocCommsData *comms_data = (InprocCommsData *)comm_block->comms_data;
    LOG("inproc_send_initial_load: starting");
    base_load_initial_content(comms_data->parser_code_buffer, initial_load);
    CB_ParseTree *result = comms_data->parser_code_buffer->parse_tree;
    comms_data->parser_code_buffer->parse_tree = NULL; 
    LOG("inproc_send_initial_load: finished");
    return result;
}

static CB_ParseTree * inproc_send_delta(CommunicationFunctions *comm_block, Delta *delta) {
    InprocCommsData *comms_data = (InprocCommsData *)comm_block->comms_data;
    LOG("inproc_send_delta: starting");
    base_replay_delta(comms_data->parser_code_buffer, delta);
    base_parse_buffer(comms_data->parser_code_buffer);
    CB_ParseTree *result = comms_data->parser_code_buffer->parse_tree;
    comms_data->parser_code_buffer->parse_tree = NULL;
    LOG("inproc_send_delta: finished");
    return result;
}

static CB_ParseTree * inproc_send_hypothesis(CommunicationFunctions *comm_block, Delta *delta) {
    InprocCommsData *comms_data = (InprocCommsData *)comm_block->comms_data;
    LOG("inproc_send_hypothesis: starting");
    CB_ParseTree *result = parse_hypothesis_on_copy(comms_data->parser_code_buffer, delta);
    LOG("inproc_send_hypothesis: finished");
    return result;
}

CommunicationFunctions* create_inproc_communication_functions(CodeBuffer *parser_cb) {
    CommunicationFunctions *comm = (CommunicationFunctions *)malloc(sizeof(CommunicationFunctions));
    comm->send_initial_load = inproc_send_initial_load;
    comm->send_delta = inproc_send_delta;
    comm->send_hypothesis = inproc_send_hypothesis;
    comm->request_ep_config = NULL;
    comm->kill_connection = NULL;
    comm->command = NULL;
    InprocCommsData *comms_data = (InprocCommsData *)malloc(sizeof(InprocCommsData));
    comms_data->comm = comm;
    comms_data->parser_code_buffer = parser_cb;
    comm->comms_data = comms_data;
    return comm;
}

void free_inproc_communication_functions(CommunicationFunctions *comm) {
    if (comm == NULL) return;
    free(comm->comms_data);
    free(comm);
}

/* --- Socket Transport Utilities --- */

static int send_all(SocketHandle sock, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        int n = send(sock, buf + total, (int)(len - total), 0);
        if (n <= 0) {
            LOG("send_all: failed, n=%d, errno=%d", n, errno);
            return -1;
        }
        total += n;
    }
    return 0;
}

static int recv_all(SocketHandle sock, char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        int n = recv(sock, buf + total, (int)(len - total), 0);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }
    return 0;
}

static char* receive_msg(SocketHandle sock) {
    char len_hex[9];
    if (recv_all(sock, len_hex, 8) < 0) {
        return NULL;
    }
    len_hex[8] = '\0';
    uint32_t len;
    if (sscanf(len_hex, "%x", &len) != 1) return NULL;
    char *buf = (char*)malloc(len + 1);
    if (recv_all(sock, buf, len) < 0) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

#ifdef _WIN32
static int ensure_winsock_initialized(void) {
    static int initialized = 0;

    if (initialized) {
        return 0;
    }

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        LOG("WSAStartup failed");
        return -1;
    }

    initialized = 1;
    return 0;
}
#endif

static int send_msg(SocketHandle sock, const char *msg) {
    uint32_t len = strlen(msg);
    char len_hex[9];
    sprintf(len_hex, "%08x", len);
    if (send_all(sock, len_hex, 8) < 0) return -1;
    return send_all(sock, msg, len);
}

/* --- Socket Client Comms --- */

typedef struct {
    char *address;
    int port;
} SocketCommsData;

static void socket_sleep_ms(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

static SocketHandle connect_socket_with_retry(const char *address, int port) {
    const int max_attempts = 20;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        SocketHandle sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;

        if (sock == INVALID_SOCKET_HANDLE) {
            return INVALID_SOCKET_HANDLE;
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, address, &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
            return sock;
        }

        CLOSE_SOCKET(sock);
        socket_sleep_ms(50);
    }

    return INVALID_SOCKET_HANDLE;
}

static CB_ParseTree* socket_send_initial_load(CommunicationFunctions *comm_block, InitialLoad *initial_load) {
    SocketCommsData *sd = (SocketCommsData*)comm_block->comms_data;
    LOG("socket_send_initial_load: connecting to %s:%d", sd->address, sd->port);
#ifdef _WIN32
    if (ensure_winsock_initialized() != 0) {
        return NULL;
    }
#endif
    SocketHandle sock = connect_socket_with_retry(sd->address, sd->port);
    if (sock == INVALID_SOCKET_HANDLE) {
        LOG("socket_send_initial_load: connect failed after retries, errno=%d", errno);
        return NULL;
    }

    char *payload = cb_serialize_initial_load(initial_load);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "I|%s", payload);
    send_msg(sock, full_req);
    free(payload);
    free(full_req);

    char *resp = receive_msg(sock);
    CLOSE_SOCKET(sock);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static CB_ParseTree* socket_send_delta(CommunicationFunctions *comm_block, Delta *delta) {
    SocketCommsData *sd = (SocketCommsData*)comm_block->comms_data;
#ifdef _WIN32
    if (ensure_winsock_initialized() != 0) {
        return NULL;
    }
#endif
    SocketHandle sock = connect_socket_with_retry(sd->address, sd->port);
    if (sock == INVALID_SOCKET_HANDLE) {
        return NULL;
    }

    char *payload = cb_serialize_delta(delta);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "D|%s", payload);
    send_msg(sock, full_req);
    free(payload);
    free(full_req);

    char *resp = receive_msg(sock);
    CLOSE_SOCKET(sock);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static CB_ParseTree* socket_send_hypothesis(CommunicationFunctions *comm_block, Delta *delta) {
    SocketCommsData *sd = (SocketCommsData*)comm_block->comms_data;
#ifdef _WIN32
    if (ensure_winsock_initialized() != 0) {
        return NULL;
    }
#endif
    SocketHandle sock = connect_socket_with_retry(sd->address, sd->port);
    if (sock == INVALID_SOCKET_HANDLE) {
        return NULL;
    }

    char *payload = cb_serialize_delta(delta);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "H|%s", payload);
    send_msg(sock, full_req);
    free(payload);
    free(full_req);

    char *resp = receive_msg(sock);
    CLOSE_SOCKET(sock);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static void socket_request_ep_config(CommunicationFunctions *comm_block) {
    SocketCommsData *sd = (SocketCommsData*)comm_block->comms_data;
    SocketHandle sock = INVALID_SOCKET_HANDLE;

#ifdef _WIN32
    if (ensure_winsock_initialized() != 0) {
        return;
    }
#endif

    sock = connect_socket_with_retry(sd->address, sd->port);
    if (sock == INVALID_SOCKET_HANDLE) {
        LOG("socket_request_ep_config: connect failed after retries, errno=%d", errno);
        return;
    }

    send_msg(sock, "C|EP");
    char *resp = receive_msg(sock);
    CLOSE_SOCKET(sock);
    
    if (resp && resp[0] == 'C' && resp[1] == '|') {
        cb_load_ep_config_from_string(resp + 2);
    }
    if (resp) free(resp);
}

CommunicationFunctions* create_socket_communication_functions(const char *address, int port) {
    CommunicationFunctions *comm = (CommunicationFunctions*)malloc(sizeof(CommunicationFunctions));
    comm->send_initial_load = socket_send_initial_load;
    comm->send_delta = socket_send_delta;
    comm->send_hypothesis = socket_send_hypothesis;
    comm->request_ep_config = socket_request_ep_config;
    comm->kill_connection = NULL;
    comm->command = NULL;
    SocketCommsData *sd = (SocketCommsData*)malloc(sizeof(SocketCommsData));
    sd->address = strdup(address);
    sd->port = port;
    comm->comms_data = sd;
    
    comm->request_ep_config(comm);
    return comm;
}

/* --- Socket Server --- */

void cb_start_server(CodeBuffer *parser_cb, const char *address, int port) {
    SocketHandle server_fd, new_socket;
    struct sockaddr_in serv_addr;
    int opt = 1;
    int addrlen = sizeof(serv_addr);
    ParserDocumentSession *sessions = NULL;

#ifdef _WIN32
    if (ensure_winsock_initialized() != 0) {
        exit(EXIT_FAILURE);
    }
#endif

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET_HANDLE) {
        LOG("cb_start_server: socket failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; 
    serv_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG("cb_start_server: bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        LOG("cb_start_server: listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Parser server listening on %s:%d\n", address, port);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&serv_addr, (socklen_t*)&addrlen)) < 0) {
            continue;
        }

        char *req = receive_msg(new_socket);
        if (req) {
            if (req[0] == 'C') {
                const char *config = cb_get_ep_config_string();
                if (!config) config = "";
                char *resp = malloc(strlen(config) + 10);
                sprintf(resp, "C|%s", config);
                send_msg(new_socket, resp);
                free(resp);
                free(req);
                CLOSE_SOCKET(new_socket);
                continue;
            }

            CB_ParseTree *tree = handle_parser_request(parser_cb, &sessions, req);
            CB_TokenStream *stream = cb_flatten_tree(tree);
            char *resp = cb_serialize_token_stream(stream);
            send_msg(new_socket, resp);
            
            free(resp);
            if (req[0] == 'H' && tree) cb_free_token_buffer(tree);
            if (stream) cb_free_token_stream(stream);
            free(req);
        }
        CLOSE_SOCKET(new_socket);
    }

    free_parser_sessions(sessions);
}

/* --- STDIN/STDOUT (Pipe) Client Comms --- */

#ifndef _WIN32
typedef struct {
    int read_fd;
    int write_fd;
    pid_t pid;
} StdioCommsData;

static int write_all(int fd, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, buf + total, len - total);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        total += n;
    }
    return 0;
}

static int read_all(int fd, char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, buf + total, len - total);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        total += n;
    }
    return 0;
}

static int stdio_send_msg(int fd, const char *msg) {
    uint32_t len = strlen(msg);
    char len_hex[9];
    sprintf(len_hex, "%08x", len);
    if (write_all(fd, len_hex, 8) < 0) return -1;
    return write_all(fd, msg, len);
}

static char* stdio_receive_msg(int fd) {
    char len_hex[9];
    if (read_all(fd, len_hex, 8) < 0) return NULL;
    len_hex[8] = '\0';
    uint32_t len;
    if (sscanf(len_hex, "%x", &len) != 1) return NULL;
    char *buf = (char*)malloc(len + 1);
    if (read_all(fd, buf, len) < 0) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

static CB_ParseTree* stdio_send_initial_load(CommunicationFunctions *comm_block, InitialLoad *initial_load) {
    StdioCommsData *sd = (StdioCommsData*)comm_block->comms_data;
    char *payload = cb_serialize_initial_load(initial_load);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "I|%s", payload);
    stdio_send_msg(sd->write_fd, full_req);
    free(payload);
    free(full_req);

    char *resp = stdio_receive_msg(sd->read_fd);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static CB_ParseTree* stdio_send_delta(CommunicationFunctions *comm_block, Delta *delta) {
    StdioCommsData *sd = (StdioCommsData*)comm_block->comms_data;
    char *payload = cb_serialize_delta(delta);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "D|%s", payload);
    stdio_send_msg(sd->write_fd, full_req);
    free(payload);
    free(full_req);

    char *resp = stdio_receive_msg(sd->read_fd);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static CB_ParseTree* stdio_send_hypothesis(CommunicationFunctions *comm_block, Delta *delta) {
    StdioCommsData *sd = (StdioCommsData*)comm_block->comms_data;
    char *payload = cb_serialize_delta(delta);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "H|%s", payload);
    stdio_send_msg(sd->write_fd, full_req);
    free(payload);
    free(full_req);

    char *resp = stdio_receive_msg(sd->read_fd);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static void stdio_request_ep_config(CommunicationFunctions *comm_block) {
    StdioCommsData *sd = (StdioCommsData*)comm_block->comms_data;
    stdio_send_msg(sd->write_fd, "C|EP");
    char *resp = stdio_receive_msg(sd->read_fd);
    if (resp && resp[0] == 'C' && resp[1] == '|') {
        cb_load_ep_config_from_string(resp + 2);
    }
    if (resp) free(resp);
}

static void stdio_kill_connection(CommunicationFunctions *comm_block) {
    if (!comm_block || !comm_block->comms_data) return;
    StdioCommsData *sd = (StdioCommsData*)comm_block->comms_data;
    if (sd->pid > 0) {
        kill(sd->pid, SIGKILL);
        sd->pid = -1;
    }
    if (sd->read_fd >= 0) { close(sd->read_fd); sd->read_fd = -1; }
    if (sd->write_fd >= 0) { close(sd->write_fd); sd->write_fd = -1; }
}

CommunicationFunctions* create_stdio_communication_functions(const char *command) {
    int pipe_in[2];  /* Editor -> Parser */
    int pipe_out[2]; /* Parser -> Editor */

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
        perror("pipe failed");
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return NULL;
    }

    if (pid == 0) {
        /* Child: Parser */
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);

        /* Split command into args for execvp */
        char *cmd_copy = strdup(command);
        char *argv[10];
        int i = 0;
        char *token = strtok(cmd_copy, " ");
        while (token && i < 9) {
            argv[i++] = token;
            token = strtok(NULL, " ");
        }
        argv[i] = NULL;

        execvp(argv[0], argv);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        /* Parent: Editor */
        close(pipe_in[0]);
        close(pipe_out[1]);

        CommunicationFunctions *comm = (CommunicationFunctions*)malloc(sizeof(CommunicationFunctions));
        comm->send_initial_load = stdio_send_initial_load;
        comm->send_delta = stdio_send_delta;
        comm->send_hypothesis = stdio_send_hypothesis;
        comm->request_ep_config = stdio_request_ep_config;
        comm->kill_connection = stdio_kill_connection;
        comm->command = command ? strdup(command) : NULL;
        StdioCommsData *sd = (StdioCommsData*)malloc(sizeof(StdioCommsData));
        sd->read_fd = pipe_out[0];
        sd->write_fd = pipe_in[1];
        sd->pid = pid;
        comm->comms_data = sd;
        
        comm->request_ep_config(comm);
        return comm;
    }
}

void free_stdio_communication_functions(CommunicationFunctions *comm) {
    if (comm == NULL) return;
    StdioCommsData *sd = (StdioCommsData*)comm->comms_data;
    if (sd) {
        close(sd->read_fd);
        close(sd->write_fd);
        kill(sd->pid, SIGTERM);
        waitpid(sd->pid, NULL, 0);
        free(sd);
    }
    if (comm->command) free(comm->command);
    free(comm);
}

void cb_start_stdio_server(CodeBuffer *parser_cb) {
    LOG("Parser stdio server starting");
    ParserDocumentSession *sessions = NULL;

    while (1) {
        char *req = stdio_receive_msg(STDIN_FILENO);
        if (!req) {
            LOG("cb_start_stdio_server: receive_msg failed or EOF");
            break;
        }

        if (req[0] == 'C') {
            const char *config = cb_get_ep_config_string();
            if (!config) config = "";
            char *resp = malloc(strlen(config) + 10);
            sprintf(resp, "C|%s", config);
            stdio_send_msg(STDOUT_FILENO, resp);
            free(resp);
            free(req);
            continue;
        }

        CB_ParseTree *tree = handle_parser_request(parser_cb, &sessions, req);
        CB_TokenStream *stream = cb_flatten_tree(tree);
        char *resp = cb_serialize_token_stream(stream);
        stdio_send_msg(STDOUT_FILENO, resp);
        
        free(resp);
        if (req[0] == 'H' && tree) cb_free_token_buffer(tree);
        if (stream) cb_free_token_stream(stream);
        free(req);
    }
    free_parser_sessions(sessions);
}
#else
/* Windows STDIN/STDOUT Implementation */
typedef struct {
    HANDLE read_handle;
    HANDLE write_handle;
    HANDLE process_handle;
} WinStdioCommsData;

static int win_write_all(HANDLE h, const char *buf, size_t len) {
    DWORD total = 0;
    while (total < len) {
        DWORD n = 0;
        if (!WriteFile(h, buf + total, (DWORD)(len - total), &n, NULL) || n == 0) {
            return -1;
        }
        total += n;
    }
    return 0;
}

static int win_read_all(HANDLE h, char *buf, size_t len) {
    DWORD total = 0;
    while (total < len) {
        DWORD n = 0;
        if (!ReadFile(h, buf + total, (DWORD)(len - total), &n, NULL) || n == 0) {
            return -1;
        }
        total += n;
    }
    return 0;
}

static int win_stdio_send_msg(HANDLE h, const char *msg) {
    uint32_t len = strlen(msg);
    char len_hex[9];
    sprintf(len_hex, "%08x", len);
    if (win_write_all(h, len_hex, 8) < 0) return -1;
    return win_write_all(h, msg, len);
}

static char* win_stdio_receive_msg(HANDLE h) {
    char len_hex[9];
    if (win_read_all(h, len_hex, 8) < 0) return NULL;
    len_hex[8] = '\0';
    uint32_t len;
    if (sscanf(len_hex, "%x", &len) != 1) return NULL;
    char *buf = (char*)malloc(len + 1);
    if (win_read_all(h, buf, len) < 0) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

static CB_ParseTree* win_stdio_send_initial_load(CommunicationFunctions *comm_block, InitialLoad *initial_load) {
    WinStdioCommsData *sd = (WinStdioCommsData*)comm_block->comms_data;
    char *payload = cb_serialize_initial_load(initial_load);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "I|%s", payload);
    win_stdio_send_msg(sd->write_handle, full_req);
    free(payload);
    free(full_req);

    char *resp = win_stdio_receive_msg(sd->read_handle);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static CB_ParseTree* win_stdio_send_delta(CommunicationFunctions *comm_block, Delta *delta) {
    WinStdioCommsData *sd = (WinStdioCommsData*)comm_block->comms_data;
    char *payload = cb_serialize_delta(delta);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "D|%s", payload);
    win_stdio_send_msg(sd->write_handle, full_req);
    free(payload);
    free(full_req);

    char *resp = win_stdio_receive_msg(sd->read_handle);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static CB_ParseTree* win_stdio_send_hypothesis(CommunicationFunctions *comm_block, Delta *delta) {
    WinStdioCommsData *sd = (WinStdioCommsData*)comm_block->comms_data;
    char *payload = cb_serialize_delta(delta);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "H|%s", payload);
    win_stdio_send_msg(sd->write_handle, full_req);
    free(payload);
    free(full_req);

    char *resp = win_stdio_receive_msg(sd->read_handle);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static void win_stdio_request_ep_config(CommunicationFunctions *comm_block) {
    WinStdioCommsData *sd = (WinStdioCommsData*)comm_block->comms_data;
    win_stdio_send_msg(sd->write_handle, "C|EP");
    char *resp = win_stdio_receive_msg(sd->read_handle);
    if (resp && resp[0] == 'C' && resp[1] == '|') {
        cb_load_ep_config_from_string(resp + 2);
    }
    if (resp) free(resp);
}

static void win_stdio_kill_connection(CommunicationFunctions *comm_block) {
    if (!comm_block || !comm_block->comms_data) return;
    WinStdioCommsData *sd = (WinStdioCommsData*)comm_block->comms_data;
    if (sd->process_handle) {
        TerminateProcess(sd->process_handle, 1);
        CloseHandle(sd->process_handle);
        sd->process_handle = NULL;
    }
    if (sd->read_handle) { CloseHandle(sd->read_handle); sd->read_handle = NULL; }
    if (sd->write_handle) { CloseHandle(sd->write_handle); sd->write_handle = NULL; }
}

CommunicationFunctions* create_stdio_communication_functions(const char *command) {
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
    SECURITY_ATTRIBUTES saAttr;

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
        LOG("StdoutRd CreatePipe failed");
        return NULL;
    }
    if (!SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0)) {
        LOG("Stdout SetHandleInformation failed");
        return NULL;
    }
    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
        LOG("Stdin CreatePipe failed");
        return NULL;
    }
    if (!SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0)) {
        LOG("Stdin SetHandleInformation failed");
        return NULL;
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStdoutWr;
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    char *cmd_copy = strdup(command);
    if (!CreateProcess(NULL, cmd_copy, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) {
        char err_buf[256];
        sprintf(err_buf, "CreateProcess failed with error: %lu", GetLastError());
        LOG(err_buf);
        free(cmd_copy);
        return NULL;
    }
    free(cmd_copy);

    CloseHandle(hChildStdoutWr);
    CloseHandle(hChildStdinRd);
    CloseHandle(piProcInfo.hThread);

    CommunicationFunctions *comm = (CommunicationFunctions*)malloc(sizeof(CommunicationFunctions));
    comm->send_initial_load = win_stdio_send_initial_load;
    comm->send_delta = win_stdio_send_delta;
    comm->send_hypothesis = win_stdio_send_hypothesis;
    comm->request_ep_config = win_stdio_request_ep_config;
    comm->kill_connection = win_stdio_kill_connection;
    comm->command = command ? strdup(command) : NULL;
    WinStdioCommsData *sd = (WinStdioCommsData*)malloc(sizeof(WinStdioCommsData));
    sd->read_handle = hChildStdoutRd;
    sd->write_handle = hChildStdinWr;
    sd->process_handle = piProcInfo.hProcess;
    comm->comms_data = sd;

    comm->request_ep_config(comm);
    return comm;
}

void free_stdio_communication_functions(CommunicationFunctions *comm) {
    if (comm == NULL) return;
    WinStdioCommsData *sd = (WinStdioCommsData*)comm->comms_data;
    if (sd) {
        CloseHandle(sd->read_handle);
        CloseHandle(sd->write_handle);
        TerminateProcess(sd->process_handle, 0);
        CloseHandle(sd->process_handle);
        free(sd);
    }
    if (comm->command) free(comm->command);
    free(comm);
}

void cb_start_stdio_server(CodeBuffer *parser_cb) {
    LOG("Parser stdio server starting");
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    ParserDocumentSession *sessions = NULL;

    while (1) {
        char *req = win_stdio_receive_msg(hStdin);
        if (!req) {
            LOG("cb_start_stdio_server: receive_msg failed or EOF");
            break;
        }

        if (req[0] == 'C') {
            const char *config = cb_get_ep_config_string();
            if (!config) config = "";
            char *resp = malloc(strlen(config) + 10);
            sprintf(resp, "C|%s", config);
            win_stdio_send_msg(hStdout, resp);
            free(resp);
            free(req);
            continue;
        }

        CB_ParseTree *tree = handle_parser_request(parser_cb, &sessions, req);
        CB_TokenStream *stream = cb_flatten_tree(tree);
        char *resp = cb_serialize_token_stream(stream);
        win_stdio_send_msg(hStdout, resp);

        free(resp);
        if (req[0] == 'H' && tree) cb_free_token_buffer(tree);
        if (stream) cb_free_token_stream(stream);
        free(req);
    }
    free_parser_sessions(sessions);
}
#endif
