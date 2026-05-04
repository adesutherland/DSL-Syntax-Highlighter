#include <stdlib.h>
#include <string.h>

#include "c_parser.h"
#include "dslsyntax_common.h"
#include "dslsyntax_log.h"
#include "dslsyntax_parser.h"

static const char *c_ep_config =
    "[.c]\n"
    "keywords=auto,break,case,const,continue,default,do,else,enum,extern,for,goto,if,inline,register,restrict,return,sizeof,static,struct,switch,typedef,union,volatile,while,_Bool\n"
    "operators=+,-,*,/,%,=,==,!=,<,>,<=,>=,&&,||,!,&,|,^,~,<<,>>,++,--,+=,-=,*=,/=,%=,->,.,?,:,(,),{,},[,],;,\n"
    "line_comment=//\n"
    "block_start=/*\n"
    "block_end=*/\n"
    "quotes=\"'\n"
    "\n"
    "[.h]\n"
    "keywords=auto,break,case,const,continue,default,do,else,enum,extern,for,goto,if,inline,register,restrict,return,sizeof,static,struct,switch,typedef,union,volatile,while,_Bool\n"
    "operators=+,-,*,/,%,=,==,!=,<,>,<=,>=,&&,||,!,&,|,^,~,<<,>>,++,--,+=,-=,*=,/=,%=,->,.,?,:,(,),{,},[,],;,\n"
    "line_comment=//\n"
    "block_start=/*\n"
    "block_end=*/\n"
    "quotes=\"'\n";

int main(int argc, char *argv[]) {
    int port = 0;
    int debug = 0;
    int stdio_mode = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            c_slow_mode = 1;
        } else if (strcmp(argv[i], "--stdio") == 0) {
            stdio_mode = 1;
        } else if (strcmp(argv[i], "--syntaxhighlight") == 0) {
            /* Compatibility with parser_tester invocations. */
        } else {
            port = atoi(argv[i]);
            stdio_mode = 0;
        }
    }

    if (debug) cb_log_init("c_parser.log");
    cb_set_ep_config_string(c_ep_config);

    CodeBuffer *cb = create_code_buffer(NULL, c_parser);
    if (stdio_mode) {
        LOG("C Parser Server starting in stdio mode... (slow_mode=%d)", c_slow_mode);
        cb_start_stdio_server(cb);
    } else {
        LOG("C Parser Server starting on port %d... (slow_mode=%d)", port, c_slow_mode);
        cb_start_server(cb, "127.0.0.1", port);
    }

    free_code_buffer(cb);
    if (debug) cb_log_close();
    return 0;
}
