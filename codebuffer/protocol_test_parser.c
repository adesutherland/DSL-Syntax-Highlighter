#include "dslsyntax_common.h"
#include "dslsyntax_parser.h"

int main(void) {
    CodeBuffer *cb = create_code_buffer(NULL, NULL);
    cb_start_stdio_server(cb);
    free_code_buffer(cb);
    return 0;
}
