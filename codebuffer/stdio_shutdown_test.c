#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define ACCESS _access
#ifndef X_OK
#define X_OK 1
#endif
#else
#include <unistd.h>
#define ACCESS access
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

static const char *find_toy_parser(void) {
#ifdef _WIN32
    static const char *candidates[] = {
        "toyparser/tp.exe",
        "cmake-build-debug/toyparser/tp.exe",
        "../toyparser/tp.exe",
        "../../toyparser/tp.exe"
    };
#else
    static const char *candidates[] = {
        "toyparser/tp",
        "cmake-build-debug/toyparser/tp",
        "../toyparser/tp",
        "../../toyparser/tp"
    };
#endif

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (ACCESS(candidates[i], X_OK) == 0) return candidates[i];
    }
    return NULL;
}

int main(void) {
    const char *parser_path = find_toy_parser();
    char parser_cmd[1024];
    CommunicationFunctions *comm;
    CodeBuffer *cb;
    InitialLoad *initial;

    if (!parser_path) {
        printf("Skipping stdio shutdown test: toy parser executable not found.\n");
        return 0;
    }

    assert(snprintf(parser_cmd, sizeof(parser_cmd), "%s -s --syntaxhighlight", parser_path) < (int)sizeof(parser_cmd));

    editor_init();
    comm = create_stdio_communication_functions(parser_cmd);
    assert(comm != NULL);

    cb = create_code_buffer(comm, NULL);
    assert(cb != NULL);
    cb_set_auto_relaunch(cb, 0);

    initial = create_initial_load("shutdown.toy", "say hello\n");
    load_initial_content(cb, initial);
    short_sleep();

    cb_kill_parser_process(cb);
    editor_wait_for_parser_threads();
    assert(cb->async_parse_active == 0);

    cb->communication_functions = NULL;
    free_code_buffer(cb);
    free_stdio_communication_functions(comm);
    editor_free();

    printf("Stdio shutdown during active parse test passed.\n");
    return 0;
}
