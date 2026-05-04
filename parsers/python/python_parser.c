#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "dslsyntax_log.h"
#include "python_parser.h"
#include "tree_sitter_adapter.h"

const TSLanguage *tree_sitter_python(void);

int python_slow_mode = 0;

static const TSAdapterProfile python_profile = {
    "python",
    TS_ADAPTER_PYTHON
};

void python_parser(CodeBuffer *codeBuffer) {
    LOG("python_parser: starting parse");
    if (python_slow_mode) {
#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
    }
    dslsh_tree_sitter_parse(codeBuffer, tree_sitter_python(), &python_profile);
    LOG("python_parser: finished parse");
}
