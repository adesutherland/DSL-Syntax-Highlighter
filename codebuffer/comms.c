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

CommunicationFunctions* create_inproc_communication_functions(CodeBuffer *parser_cb) {
    CommunicationFunctions *comm = (CommunicationFunctions *)malloc(sizeof(CommunicationFunctions));
    comm->send_initial_load = inproc_send_initial_load;
    comm->send_delta = inproc_send_delta;
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

static CB_ParseTree* socket_send_initial_load(CommunicationFunctions *comm_block, InitialLoad *initial_load) {
    SocketCommsData *sd = (SocketCommsData*)comm_block->comms_data;
    LOG("socket_send_initial_load: connecting to %s:%d", sd->address, sd->port);
#ifdef _WIN32
    if (ensure_winsock_initialized() != 0) {
        return NULL;
    }
#endif
    SocketHandle sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(sd->port);
    inet_pton(AF_INET, sd->address, &serv_addr.sin_addr);

    if (sock == INVALID_SOCKET_HANDLE || connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG("socket_send_initial_load: connect failed, errno=%d", errno);
        if (sock != INVALID_SOCKET_HANDLE) {
            CLOSE_SOCKET(sock);
        }
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
    SocketHandle sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(sd->port);
    inet_pton(AF_INET, sd->address, &serv_addr.sin_addr);

    if (sock == INVALID_SOCKET_HANDLE || connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        if (sock != INVALID_SOCKET_HANDLE) {
            CLOSE_SOCKET(sock);
        }
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

static void socket_request_ep_config(CommunicationFunctions *comm_block) {
    SocketCommsData *sd = (SocketCommsData*)comm_block->comms_data;
    SocketHandle sock = INVALID_SOCKET_HANDLE;
    struct sockaddr_in serv_addr;

#ifdef _WIN32
    if (ensure_winsock_initialized() != 0) {
        return;
    }
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET_HANDLE) {
        LOG("socket_request_ep_config: socket creation error");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(sd->port);
    inet_pton(AF_INET, sd->address, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG("socket_request_ep_config: connect failed, errno=%d", errno);
        CLOSE_SOCKET(sock);
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
    comm->request_ep_config = socket_request_ep_config;
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
            } else if (req[0] == 'I') {
                InitialLoad *load = cb_deserialize_initial_load(req + 2);
                base_load_initial_content(parser_cb, load);
            } else if (req[0] == 'D') {
                Delta *delta = cb_deserialize_delta(req + 2);
                base_replay_delta(parser_cb, delta);
                base_parse_buffer(parser_cb);
                free_delta(delta);
            }

            CB_TokenStream *stream = cb_flatten_tree(parser_cb->parse_tree);
            char *resp = cb_serialize_token_stream(stream);
            send_msg(new_socket, resp);
            
            free(resp);
            if (stream) cb_free_token_stream(stream);
            free(req);
        }
        CLOSE_SOCKET(new_socket);
    }
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

static void stdio_request_ep_config(CommunicationFunctions *comm_block) {
    StdioCommsData *sd = (StdioCommsData*)comm_block->comms_data;
    stdio_send_msg(sd->write_fd, "C|EP");
    char *resp = stdio_receive_msg(sd->read_fd);
    if (resp && resp[0] == 'C' && resp[1] == '|') {
        cb_load_ep_config_from_string(resp + 2);
    }
    if (resp) free(resp);
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
        comm->request_ep_config = stdio_request_ep_config;
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
    free(comm);
}

void cb_start_stdio_server(CodeBuffer *parser_cb) {
    LOG("Parser stdio server starting");

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
        } else if (req[0] == 'I') {
            InitialLoad *load = cb_deserialize_initial_load(req + 2);
            base_load_initial_content(parser_cb, load);
        } else if (req[0] == 'D') {
            Delta *delta = cb_deserialize_delta(req + 2);
            base_replay_delta(parser_cb, delta);
            base_parse_buffer(parser_cb);
            free_delta(delta);
        }

        CB_TokenStream *stream = cb_flatten_tree(parser_cb->parse_tree);
        char *resp = cb_serialize_token_stream(stream);
        stdio_send_msg(STDOUT_FILENO, resp);
        
        free(resp);
        if (stream) cb_free_token_stream(stream);
        free(req);
    }
}
#else
CommunicationFunctions* create_stdio_communication_functions(const char *command) {
    (void)command;
    LOG("create_stdio_communication_functions: unsupported on Windows");
    return NULL;
}

void free_stdio_communication_functions(CommunicationFunctions *comm) {
    free(comm);
}

void cb_start_stdio_server(CodeBuffer *parser_cb) {
    (void)parser_cb;
    LOG("cb_start_stdio_server: unsupported on Windows");
}
#endif
