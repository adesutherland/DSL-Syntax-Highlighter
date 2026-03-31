#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser_highlighter.h"
#include "dslsyntax_common.h"
#include "serialization.h"
#include "dslsyntax_log.h"

int main(int argc, char *argv[]) {
    int port = 8080;
    int debug = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) debug = 1;
        else if (strcmp(argv[i], "-s") == 0) slow_mode = 1;
        else port = atoi(argv[i]);
    }

    if (debug) cb_log_init("parser.log");
    LOG("Toy Parser Server starting on port %d... (slow_mode=%d)", port, slow_mode);

    /* Create the CodeBuffer with the toy_parser function */
    CodeBuffer *cb = create_code_buffer(NULL, toy_parser);
    
    cb_start_server(cb, "127.0.0.1", port);

    /* Free resources (unreachable in this simple loop) */
    free_code_buffer(cb);
    if (debug) cb_log_close();

    return 0;
}
