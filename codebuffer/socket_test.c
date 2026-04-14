#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "dslsyntax_common.h"
#include "serialization.h"

// Define a cross-platform sleep function
void sleep_ms(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

typedef struct {
    CodeBuffer *cb;
    int port;
} SocketTestServerArgs;

static int find_available_port(void) {
    int port = -1;
#ifdef _WIN32
    SOCKET probe = INVALID_SOCKET;
#else
    int probe = -1;
#endif
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return -1;
    }
#endif

    probe = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (probe == INVALID_SOCKET) {
#else
    if (probe < 0) {
#endif
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(probe, (struct sockaddr *)&addr, sizeof(addr)) == 0 &&
        getsockname(probe, (struct sockaddr *)&addr, &addr_len) == 0) {
        port = ntohs(addr.sin_port);
    }

#ifdef _WIN32
    closesocket(probe);
#else
    close(probe);
#endif

    return port;
}

static void* server_thread_func(void* arg) {
    SocketTestServerArgs *server_args = (SocketTestServerArgs*)arg;
    cb_start_server(server_args->cb, "127.0.0.1", server_args->port);
    return NULL;
}

void test_socket_communication() {
    printf("Testing socket communication...\n");

    if (init_parser_thread_utils() != 0) {
        fprintf(stderr, "Failed to initialize thread utils.\n");
        return;
    }

    /* Create Parser CodeBuffer */
    CodeBuffer *parser_cb = create_code_buffer(NULL, NULL);
    int port = find_available_port();
    assert(port > 0);
    SocketTestServerArgs server_args = { parser_cb, port };
    
    /* Start server in a thread */
    if (launch_parser_thread(server_thread_func, &server_args) != 0) {
        fprintf(stderr, "Failed to launch server thread.\n");
        destroy_thread_utils();
        return;
    }
    
    /* Give server time to start */
    sleep_ms(100);

    /* Create Client Communication */
    CommunicationFunctions *comm = create_socket_communication_functions("127.0.0.1", port);
    if (comm == NULL) {
        fprintf(stderr, "Failed to create socket communication functions.\n");
        destroy_thread_utils();
        return;
    }

    /* 1. Test Initial Load */
    printf("Client: Sending Initial Load...\n");
    InitialLoad load;
    load.unique_document_id = strdup("test_doc");
    load.change_version = 1;
    load.line_count = 2;
    load.lines = (CodeBufferLine*)malloc(2 * sizeof(CodeBufferLine));
    utf8_to_line("Line 1", &load.lines[0]);
    utf8_to_line("Line 2", &load.lines[1]);

    CB_ParseTree *tb = comm->send_initial_load(comm, &load);
    assert(tb != NULL);
    if (tb) {
        printf("Client: Initial Load Result received. Tree root type: %d\n", tb->root->type);
    } else {
        printf("Client: Initial Load failed.\n");
    }

    /* 2. Test Delta */
    if (tb) {
        printf("Client: Sending Delta...\n");
        Delta delta;
        delta.change_version = 2;
        delta.transaction_count = 1;
        delta.transactions = (Transaction*)malloc(sizeof(Transaction));
        delta.transactions[0].type = TRANSACTION_ADDCHARS;
        delta.transactions[0].pos_line = 0;
        delta.transactions[0].pos_col = 6;
        delta.transactions[0].count = 0;
        delta.transactions[0].content = strdup(" Added");

        CB_ParseTree *tb2 = comm->send_delta(comm, &delta);
        assert(tb2 != NULL);
        if (tb2) {
            printf("Client: Delta Result received.\n");
            cb_free_token_buffer(tb2);
        } else {
            printf("Client: Delta send failed.\n");
        }
    }

    /* Cleanup client */
    if (tb) cb_free_token_buffer(tb);
    free(comm->comms_data);
    free(comm);
    
    printf("Socket communication test successful!\n");
    
    destroy_thread_utils(); // Clean up threading resources
}

int main() {
    test_socket_communication();
    return 0;
}
