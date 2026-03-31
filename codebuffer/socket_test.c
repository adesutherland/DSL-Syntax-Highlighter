#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include "dslsyntax_common.h"
#include "serialization.h"

void* server_thread_func(void* arg) {
    CodeBuffer *cb = (CodeBuffer*)arg;
    cb_start_server(cb, "127.0.0.1", 9999);
    return NULL;
}

void test_socket_communication() {
    printf("Testing socket communication...\n");

    /* Create Parser CodeBuffer */
    CodeBuffer *parser_cb = create_code_buffer(NULL, NULL);
    
    /* Start server in a thread */
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_thread_func, parser_cb);
    
    /* Give server time to start */
    usleep(100000);

    /* Create Client Communication */
    CommunicationFunctions *comm = create_socket_communication_functions("127.0.0.1", 9999);
    
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
    printf("Client: Initial Load Result received. Tree root type: %d\n", tb->root->type);

    /* 2. Test Delta */
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
    printf("Client: Delta Result received.\n");

    /* Cleanup client */
    cb_free_token_buffer(tb);
    cb_free_token_buffer(tb2);
    free(comm->comms_data);
    free(comm);
    
    printf("Socket communication test successful!\n");
    
    /* We can't easily stop the server thread in this simple test, but that's okay for a test run */
}

int main() {
    test_socket_communication();
    return 0;
}
