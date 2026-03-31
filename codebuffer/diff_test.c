#include <stdio.h>
#include <assert.h>
#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"
#include "diff.h"

void test_diff() {
    printf("Testing diff algorithm...\n");
    
    /* Setup initial buffer */
    CodeBuffer *cb = create_code_buffer(NULL, NULL);
    cb->line_count = 2;
    cb->lines = (CodeBufferLine*)malloc(2 * sizeof(CodeBufferLine));
    utf8_to_line("Line 1", &cb->lines[0]);
    utf8_to_line("Line 2", &cb->lines[1]);
    snapshot(cb); /* Create initial snapshot */

    /* New state:
       Line 1
       Line 1.5 (Added)
       Line 2 (Keep)
       Line 3 (Added)
    */
    size_t new_count = 4;
    CodeBufferLine *new_lines = (CodeBufferLine*)malloc(new_count * sizeof(CodeBufferLine));
    utf8_to_line("Line 1", &new_lines[0]);
    utf8_to_line("Line 1.5", &new_lines[1]);
    utf8_to_line("Line 2", &new_lines[2]);
    utf8_to_line("Line 3", &new_lines[3]);

    printf("Generating diff...\n");
    cb_generate_diff_transactions(cb, cb->lines, cb->line_count, new_lines, new_count);

    printf("Transaction count: %zu\n", cb->transaction_count);
    /* 
       Depending on LCS path, could be:
       ADD Line 1.5 at pos 1
       ADD Line 3 at pos 3 (after Line 2)
       Total: 2 transactions
    */
    assert(cb->transaction_count == 2);
    assert(cb->line_count == 4);
    
    char *s1 = line_to_utf8(&cb->lines[1]);
    assert(strcmp(s1, "Line 1.5") == 0);
    free(s1);

    char *s3 = line_to_utf8(&cb->lines[3]);
    assert(strcmp(s3, "Line 3") == 0);
    free(s3);

    printf("Diff test successful!\n");

    /* Cleanup */
    for (size_t i = 0; i < new_count; i++) free(new_lines[i].characters);
    free(new_lines);
    free_code_buffer(cb);
}

int main() {
    test_diff();
    return 0;
}
