#ifndef DSLSYNTAX_DIFF_H
#define DSLSYNTAX_DIFF_H

#include "dslsyntax_common.h"

/*
 * Compares two sets of lines and generates a sequence of transactions 
 * to transform 'old_lines' into 'new_lines'.
 * 
 * This is used for bulk updates (e.g., pasting or external file changes)
 * where individual keystrokes weren't captured.
 */
void cb_generate_diff_transactions(CodeBuffer *cb, 
                                   CodeBufferLine *old_lines, size_t old_count,
                                   CodeBufferLine *new_lines, size_t new_count);

#endif /* DSLSYNTAX_DIFF_H */
