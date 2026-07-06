#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"

static void short_sleep(void) {
#ifdef _WIN32
    Sleep(50);
#else
    usleep(50000);
#endif
}

int main(int argc, char **argv) {
    char parser_cmd[2048];
    int written;
    CommunicationFunctions *comm;
    CodeBuffer *cb;
    InitialLoad *initial;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <jsp>\n", argv[0]);
        return 2;
    }

#ifdef _WIN32
    written = snprintf(parser_cmd, sizeof(parser_cmd), "\"%s\" -s --syntaxhighlight", argv[1]);
#else
    written = snprintf(parser_cmd, sizeof(parser_cmd), "'%s' -s --syntaxhighlight", argv[1]);
#endif
    assert(written >= 0 && written < (int)sizeof(parser_cmd));

    editor_init();
    comm = create_stdio_communication_functions(parser_cmd);
    assert(comm != NULL);

    cb = create_code_buffer(comm, NULL);
    assert(cb != NULL);
    cb_set_auto_relaunch(cb, 0);

    initial = create_initial_load("parser-test.js",
                                  "import { readFile } from \"fs\";\n"
                                  "function greet(name) {\n"
                                  "  const value = 42;\n"
                                  "  return `hello ${name}`;\n"
                                  "}\n");
    load_initial_content(cb, initial);
    short_sleep();

    cb_kill_parser_process(cb);
    editor_wait_for_parser_threads();
    assert(cb->async_parse_active == 0);

    cb->communication_functions = NULL;
    free_code_buffer(cb);
    free_stdio_communication_functions(comm);
    editor_free();

    printf("JavaScript parser shutdown during active parse test passed.\n");
    return 0;
}
