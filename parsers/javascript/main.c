#include <stdlib.h>
#include <string.h>

#include "dslsyntax_common.h"
#include "dslsyntax_log.h"
#include "dslsyntax_parser.h"
#include "javascript_parser.h"

static const char *javascript_ep_config =
    "[.js]\n"
    "keywords=async,await,break,case,catch,class,const,continue,debugger,default,delete,do,else,export,extends,finally,for,from,function,get,if,import,in,instanceof,let,new,of,return,set,static,super,switch,this,throw,try,typeof,var,void,while,with,yield,true,false,null,undefined\n"
    "operators=+,-,*,/,%,**,=,==,===,!=,!==,<,>,<=,>=,&&,||,!,?,:,=>,.,(,),[,],{,},;,\n"
    "line_comment=//\n"
    "block_start=/*\n"
    "block_end=*/\n"
    "quotes=\"'`\n"
    "\n"
    "[.mjs]\n"
    "keywords=async,await,break,case,catch,class,const,continue,debugger,default,delete,do,else,export,extends,finally,for,from,function,get,if,import,in,instanceof,let,new,of,return,set,static,super,switch,this,throw,try,typeof,var,void,while,with,yield,true,false,null,undefined\n"
    "operators=+,-,*,/,%,**,=,==,===,!=,!==,<,>,<=,>=,&&,||,!,?,:,=>,.,(,),[,],{,},;,\n"
    "line_comment=//\n"
    "block_start=/*\n"
    "block_end=*/\n"
    "quotes=\"'`\n"
    "\n"
    "[.cjs]\n"
    "keywords=async,await,break,case,catch,class,const,continue,debugger,default,delete,do,else,export,extends,finally,for,from,function,get,if,import,in,instanceof,let,new,of,return,set,static,super,switch,this,throw,try,typeof,var,void,while,with,yield,true,false,null,undefined\n"
    "operators=+,-,*,/,%,**,=,==,===,!=,!==,<,>,<=,>=,&&,||,!,?,:,=>,.,(,),[,],{,},;,\n"
    "line_comment=//\n"
    "block_start=/*\n"
    "block_end=*/\n"
    "quotes=\"'`\n";

int main(int argc, char *argv[]) {
    int port = 0;
    int debug = 0;
    int stdio_mode = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            javascript_slow_mode = 1;
        } else if (strcmp(argv[i], "--stdio") == 0) {
            stdio_mode = 1;
        } else if (strcmp(argv[i], "--syntaxhighlight") == 0) {
            /* Compatibility with parser_tester invocations. */
        } else {
            port = atoi(argv[i]);
            stdio_mode = 0;
        }
    }

    if (debug) cb_log_init("javascript_parser.log");
    cb_set_ep_config_string(javascript_ep_config);

    CodeBuffer *cb = create_code_buffer(NULL, javascript_parser);
    if (stdio_mode) {
        LOG("JavaScript Parser Server starting in stdio mode... (slow_mode=%d)", javascript_slow_mode);
        cb_start_stdio_server(cb);
    } else {
        LOG("JavaScript Parser Server starting on port %d... (slow_mode=%d)", port, javascript_slow_mode);
        cb_start_server(cb, "127.0.0.1", port);
    }

    free_code_buffer(cb);
    if (debug) cb_log_close();
    return 0;
}
