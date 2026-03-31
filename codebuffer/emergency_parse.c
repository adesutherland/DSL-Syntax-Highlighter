#include "dslsyntax_common.h"
#include "dslsyntax_log.h"

/* Helper to get absolute position from line/col */
static size_t get_abs_pos(CodeBuffer *cb, int line, int col) {
    size_t pos = 0;
    for (int i = 0; i < line && i < (int)cb->line_count; i++) {
        pos += cb->lines[i].length + 1;
    }
    pos += col;
    return pos;
}

/* Helper to shift all nodes in the tree */
static void shift_nodes(CB_Node *node, size_t pos, int shift) {
    if (!node) return;
    
    /* If node starts after pos, shift it */
    if (node->pos >= pos) {
        node->pos += shift;
    } 
    /* If node contains pos, extend it (if it's not a shift-only operation) */
    else if (node->pos < pos && node->pos + node->length >= pos) {
        node->length += shift;
    }

    CB_Node *child = node->child;
    while (child) {
        shift_nodes(child, pos, shift);
        child = child->sibling;
    }
}

/* 
 * Lightweight line-based heuristic scan.
 * This runs after every edit to "patch" the highlighting until the parser returns.
 */
static void cb_emergency_scan_line(CodeBuffer *cb, int line_idx) {
    if (line_idx < 0 || line_idx >= (int)cb->line_count) return;
    CodeBufferLine *line = &cb->lines[line_idx];
    
    char in_string = 0;
    char string_char = 0;
    char in_comment = 0;

    for (size_t i = 0; i < line->length; i++) {
        CodeBufferCharacter *c = &line->characters[i];
        char32_t cp = c->character[0];

        /* 1. Handle Strings */
        if (!in_comment) {
            if (!in_string && (cp == '"' || cp == '\'')) {
                in_string = 1;
                string_char = (char)cp;
                c->token_type = LEXER_STRING_LITERAL;
                continue;
            } else if (in_string && cp == (char32_t)string_char) {
                in_string = 0;
                c->token_type = LEXER_STRING_LITERAL;
                continue;
            }
            if (in_string) {
                c->token_type = LEXER_STRING_LITERAL;
                continue;
            }
        }

        /* 2. Handle Comments (// and slash-star) */
        if (!in_string && !in_comment && i + 1 < line->length) {
            char32_t next_cp = line->characters[i+1].character[0];
            if (cp == '/' && (next_cp == '/' || next_cp == '*')) {
                in_comment = 1;
            }
        }
        if (in_comment) {
            c->token_type = LEXER_COMMENT;
            continue;
        }

        /* 3. Handle Numbers (only if not in string/comment) */
        if (!in_string && !in_comment) {
            if (cp >= '0' && cp <= '9') {
                c->token_type = LEXER_NUMBER_LITERAL;
                continue;
            }
        }

        /* 4. Whitespace already handled in base_apply_transaction, 
              but we reset others to default if not matched by heuristics 
              and they were previously one of the types we manage. */
        if (c->token_type == LEXER_STRING_LITERAL || 
            c->token_type == LEXER_COMMENT || 
            c->token_type == LEXER_NUMBER_LITERAL) {
            /* If the heuristic no longer applies, we could revert to LEXER_TOKEN 
               but it might be better to leave it until the real parser fixes it 
               to avoid "flicker" for keywords. */
        }
    }
}

void cb_emergency_parse_transaction(CodeBuffer *cb, Transaction transaction) {
    if (!cb || !cb->parse_tree || !cb->parse_tree->root) return;

    size_t abs_pos = get_abs_pos(cb, transaction.pos_line, transaction.pos_col);
    int shift = 0;
    size_t content_len = transaction.content ? strlen(transaction.content) : 0;

    LOG("cb_emergency_parse_transaction: type=%c, pos=%zu", transaction.type, abs_pos);

    switch (transaction.type) {
        case TRANSACTION_ADDCHARS:
            shift = (int)content_len;
            break;
        case TRANSACTION_DELETECHARS:
            shift = -transaction.count;
            break;
        case TRANSACTION_ADDLINE:
            shift = (int)content_len + 1;
            break;
        case TRANSACTION_DELETELINE:
            /* Note: shift for DELETELINE is handled by the tree being out of sync,
               but we can't easily calculate line length here without more state. */
            break;
        default:
            break;
    }

    if (shift != 0) {
        shift_nodes(cb->parse_tree->root, abs_pos, shift);
    }

    /* Apply line-based heuristics to the modified line */
    cb_emergency_scan_line(cb, transaction.pos_line);
}
