#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "dslsyntax_common.h"
#include "dslsyntax_parser.h"
#include "serialization.h"
#include "dslsyntax_log.h"

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

static int send_all(int sock, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sock, buf + total, len - total, 0);
        if (n <= 0) {
            LOG("send_all: failed, n=%zd, errno=%d", n, errno);
            return -1;
        }
        total += n;
    }
    return 0;
}

static char* receive_msg(int sock) {
    char len_hex[9];
    if (recv(sock, len_hex, 8, MSG_WAITALL) != 8) {
        return NULL;
    }
    len_hex[8] = '\0';
    uint32_t len;
    if (sscanf(len_hex, "%x", &len) != 1) return NULL;
    char *buf = (char*)malloc(len + 1);
    if (recv(sock, buf, len, MSG_WAITALL) != (ssize_t)len) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

static int send_msg(int sock, const char *msg) {
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
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(sd->port);
    inet_pton(AF_INET, sd->address, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG("socket_send_initial_load: connect failed, errno=%d", errno);
        close(sock);
        return NULL;
    }

    char *payload = cb_serialize_initial_load(initial_load);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "I|%s", payload);
    send_msg(sock, full_req);
    free(payload);
    free(full_req);

    char *resp = receive_msg(sock);
    close(sock);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

static CB_ParseTree* socket_send_delta(CommunicationFunctions *comm_block, Delta *delta) {
    SocketCommsData *sd = (SocketCommsData*)comm_block->comms_data;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(sd->port);
    inet_pton(AF_INET, sd->address, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return NULL;
    }

    char *payload = cb_serialize_delta(delta);
    char *full_req = (char*)malloc(strlen(payload) + 10);
    sprintf(full_req, "D|%s", payload);
    send_msg(sock, full_req);
    free(payload);
    free(full_req);

    char *resp = receive_msg(sock);
    close(sock);
    if (!resp) return NULL;

    CB_TokenStream *stream = cb_deserialize_token_stream(resp);
    free(resp);
    CB_ParseTree *tb = cb_reconstruct_tree(stream);
    if (stream) cb_free_token_stream(stream);
    return tb;
}

CommunicationFunctions* create_socket_communication_functions(const char *address, int port) {
    CommunicationFunctions *comm = (CommunicationFunctions*)malloc(sizeof(CommunicationFunctions));
    comm->send_initial_load = socket_send_initial_load;
    comm->send_delta = socket_send_delta;
    SocketCommsData *sd = (SocketCommsData*)malloc(sizeof(SocketCommsData));
    sd->address = strdup(address);
    sd->port = port;
    comm->comms_data = sd;
    return comm;
}

/* --- Socket Server --- */

void cb_start_server(CodeBuffer *parser_cb, const char *address, int port) {
    int server_fd, new_socket;
    struct sockaddr_in serv_addr;
    int opt = 1;
    int addrlen = sizeof(serv_addr);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        LOG("cb_start_server: socket failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
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
            if (req[0] == 'I') {
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
        close(new_socket);
    }
}

/* --- STDIN/STDOUT (Pipe) Client Comms --- */

#include <sys/wait.h>

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
        StdioCommsData *sd = (StdioCommsData*)malloc(sizeof(StdioCommsData));
        sd->read_fd = pipe_out[0];
        sd->write_fd = pipe_in[1];
        sd->pid = pid;
        comm->comms_data = sd;
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

        if (req[0] == 'I') {
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
