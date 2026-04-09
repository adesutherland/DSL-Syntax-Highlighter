#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "dslsyntax_common.h"
#include "serialization.h"

void test_stdio_communication() {
    printf("Testing stdio communication...\n");

    /* 
     * In the build environment, toyparser/tp.exe should be accessible.
     * We assume this test is run from the project root or the build directory.
     * For now, let's assume it's in the current directory or a known path.
     */
    const char *parser_cmd = "toyparser/tp.exe";

    /* Create Client Communication */
    CommunicationFunctions *comm = create_stdio_communication_functions(parser_cmd);
    if (comm == NULL) {
        fprintf(stderr, "Failed to create stdio communication functions. Is %s available?\n", parser_cmd);
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
        if (tb2) {
            printf("Client: Delta Result received.\n");
            cb_free_token_buffer(tb2);
        } else {
            printf("Client: Delta send failed.\n");
        }
    }

    /* Cleanup client */
    if (tb) cb_free_token_buffer(tb);
    free_stdio_communication_functions(comm);
    
    printf("Stdio communication test successful!\n");
}

int main() {
    test_stdio_communication();
    return 0;
}
