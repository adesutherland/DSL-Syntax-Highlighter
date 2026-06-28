#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser_highlighter.h"
#include "dslsyntax_common.h"
#include "serialization.h"
#include "dslsyntax_log.h"

int main(int argc, char *argv[]) {
    int port = 0;
    int debug = 0;
#ifdef _WIN32
    int stdio_mode = 1; /* Default to stdio */
    port = 8080;
#else
    int stdio_mode = 1; /* Default to stdio */
#endif
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) debug = 1;
        else if (strcmp(argv[i], "-s") == 0) slow_mode = 1;
        else if (strcmp(argv[i], "--stdio") == 0) {
            stdio_mode = 1;
        }
        else if (strcmp(argv[i], "--syntaxhighlight") == 0) {
            /* Ignore, compatibility with CREXX parser test scripts. */
        }
        else {
            port = atoi(argv[i]);
            stdio_mode = 0; /* If port provided, use socket */
        }
    }

    if (debug) cb_log_init("parser.log");
    
    if (stdio_mode) {
        LOG("Toy Parser Server starting in stdio mode... (slow_mode=%d)", slow_mode);
    } else {
        LOG("Toy Parser Server starting on port %d... (slow_mode=%d)", port, slow_mode);
    }

    const char *toy_config = 
        "[.toy]\n"
        "keywords=say,int,function,void,call,namespace\n"
        "operators=+,-,*,/,=,:,(,),{,},\\,\n"
        "line_comment=//,#\n"
        "block_start=/*\n"
        "block_end=*/\n"
        "ident_extra_chars=$\n"
        "quotes=\"\n";
    cb_set_ep_config_string(toy_config);

    /* Create the CodeBuffer with the toy_parser function */
    CodeBuffer *cb = create_code_buffer(NULL, toy_parser);
    
    if (stdio_mode) {
        cb_start_stdio_server(cb);
    } else {
        cb_start_server(cb, "127.0.0.1", port);
    }

    /* Free resources (unreachable in this simple loop) */
    free_code_buffer(cb);
    if (debug) cb_log_close();

    return 0;
}
