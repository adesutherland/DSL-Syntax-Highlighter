#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "c_parser.h"
#include "dslsyntax_log.h"
#include "tree_sitter_adapter.h"

const TSLanguage *tree_sitter_c(void);

int c_slow_mode = 0;

static const TSAdapterProfile c_profile = {
    "c",
    TS_ADAPTER_C
};

void c_parser(CodeBuffer *codeBuffer) {
    LOG("c_parser: starting parse");
    if (c_slow_mode) {
#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
    }
    dslsh_tree_sitter_parse(codeBuffer, tree_sitter_c(), &c_profile);
    LOG("c_parser: finished parse");
}
