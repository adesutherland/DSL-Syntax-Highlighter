#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "dslsyntax_log.h"
#include "javascript_parser.h"
#include "tree_sitter_adapter.h"

const TSLanguage *tree_sitter_javascript(void);

int javascript_slow_mode = 0;

static const TSAdapterProfile javascript_profile = {
    "javascript",
    TS_ADAPTER_JAVASCRIPT
};

void javascript_parser(CodeBuffer *codeBuffer) {
    LOG("javascript_parser: starting parse");
    if (javascript_slow_mode) {
#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
    }
    dslsh_tree_sitter_parse(codeBuffer, tree_sitter_javascript(), &javascript_profile);
    LOG("javascript_parser: finished parse");
}
