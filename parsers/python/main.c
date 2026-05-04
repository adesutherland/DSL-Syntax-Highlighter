#include <stdlib.h>
#include <string.h>

#include "dslsyntax_common.h"
#include "dslsyntax_log.h"
#include "dslsyntax_parser.h"
#include "python_parser.h"

static const char *python_ep_config =
    "[.py]\n"
    "keywords=and,as,assert,async,await,break,case,class,continue,def,del,elif,else,except,False,finally,for,from,global,if,import,in,is,lambda,match,None,nonlocal,not,or,pass,raise,return,True,try,while,with,yield\n"
    "operators=+,-,*,/,%,**,//,=,==,!=,<,>,<=,>=,:,(,),[,],{,},.,\n"
    "line_comment=#\n"
    "quotes=\"'\n";

int main(int argc, char *argv[]) {
    int port = 0;
    int debug = 0;
    int stdio_mode = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            python_slow_mode = 1;
        } else if (strcmp(argv[i], "--stdio") == 0) {
            stdio_mode = 1;
        } else if (strcmp(argv[i], "--syntaxhighlight") == 0) {
            /* Compatibility with parser_tester invocations. */
        } else {
            port = atoi(argv[i]);
            stdio_mode = 0;
        }
    }

    if (debug) cb_log_init("python_parser.log");
    cb_set_ep_config_string(python_ep_config);

    CodeBuffer *cb = create_code_buffer(NULL, python_parser);
    if (stdio_mode) {
        LOG("Python Parser Server starting in stdio mode... (slow_mode=%d)", python_slow_mode);
        cb_start_stdio_server(cb);
    } else {
        LOG("Python Parser Server starting on port %d... (slow_mode=%d)", port, python_slow_mode);
        cb_start_server(cb, "127.0.0.1", port);
    }

    free_code_buffer(cb);
    if (debug) cb_log_close();
    return 0;
}
