#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diff.h"
#include "dslsyntax_editor.h"

/*
 * Helper: compare two CodeBufferLines
 */
static int lines_equal(CodeBufferLine *l1, CodeBufferLine *l2) {
    if (l1->length != l2->length) return 0;
    for (size_t i = 0; i < l1->length; i++) {
        /* Compare first codepoint for now */
        if (l1->characters[i].character[0] != l2->characters[i].character[0])
            return 0;
    }
    return 1;
}

/*
 * Myers' Diff Algorithm (Simplistic Implementation for C)
 * 
 * We use a standard D-path algorithm to find the shortest edit script.
 * For now, to keep it simple and robust, we'll implement a 
 * greedy approach or a simpler LCS (Longest Common Subsequence) based diff.
 * 
 * Let's use a standard LCS approach which is O(N*M) - fine for typical 
 * file sizes.
 */

void cb_generate_diff_transactions(CodeBuffer *cb, 
                                   CodeBufferLine *old_lines, size_t old_count,
                                   CodeBufferLine *new_lines, size_t new_count) {
    
    /* 
     * Simple LCS DP Table
     * dp[i][j] = length of LCS between old_lines[0..i-1] and new_lines[0..j-1]
     */
    int **dp = (int**)malloc((old_count + 1) * sizeof(int*));
    for (size_t i = 0; i <= old_count; i++) {
        dp[i] = (int*)calloc(new_count + 1, sizeof(int));
    }

    for (size_t i = 1; i <= old_count; i++) {
        for (size_t j = 1; j <= new_count; j++) {
            if (lines_equal(&old_lines[i-1], &new_lines[j-1])) {
                dp[i][j] = dp[i-1][j-1] + 1;
            } else {
                dp[i][j] = (dp[i-1][j] > dp[i][j-1]) ? dp[i-1][j] : dp[i][j-1];
            }
        }
    }

    /* 
     * Backtrack to find differences and generate transactions.
     * We need to be careful: TRANSACTION_ADDLINE, TRANSACTION_DELETELINE.
     * To keep indices correct, we apply them in reverse or calculate indices carefully.
     * Applying transactions via editor_apply_transaction updates cb->lines.
     * 
     * Actually, if we use editor_apply_transaction, it modifies cb->lines *during* the loop.
     * This is dangerous if we are using old_lines/new_lines pointers.
     * 
     * Safer: build a list of transactions first, then apply.
     */

    typedef struct {
        TransactionType type;
        int pos;
        char *content;
    } DiffOp;

    DiffOp *ops = NULL;
    size_t op_count = 0;

    int i = (int)old_count;
    int j = (int)new_count;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && lines_equal(&old_lines[i-1], &new_lines[j-1])) {
            i--; j--;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            /* Addition in new_lines */
            ops = (DiffOp*)realloc(ops, (op_count + 1) * sizeof(DiffOp));
            ops[op_count].type = TRANSACTION_ADDLINE;
            ops[op_count].pos = i; /* Insert at 'i' in the 'old' coordinate system */
            ops[op_count].content = line_to_utf8(&new_lines[j-1]);
            op_count++;
            j--;
        } else if (i > 0 && (j == 0 || dp[i][j-1] < dp[i-1][j])) {
            /* Deletion from old_lines */
            ops = (DiffOp*)realloc(ops, (op_count + 1) * sizeof(DiffOp));
            ops[op_count].type = TRANSACTION_DELETELINE;
            ops[op_count].pos = i - 1;
            ops[op_count].content = NULL;
            op_count++;
            i--;
        }
    }

    /* 
     * Now apply transactions. 
     * Deletions should be applied from bottom to top to keep indices stable.
     * Additions are also sensitive to indices.
     * 
     * Actually, the backtracking above naturally gives us reverse order if we 
     * just follow it.
     */
    for (size_t k = 0; k < op_count; k++) {
        Transaction txn;
        txn.type = ops[k].type;
        txn.pos_line = ops[k].pos;
        txn.pos_col = 0;
        txn.count = 1;
        txn.content = ops[k].content;
        
        editor_apply_transaction(cb, txn);
        if (ops[k].content) free(ops[k].content);
    }

    /* Cleanup DP table */
    for (size_t i = 0; i <= old_count; i++) free(dp[i]);
    free(dp);
    free(ops);
}
