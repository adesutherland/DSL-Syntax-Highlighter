#include "dslsyntax_common.h"
#include "dslsyntax_log.h"
#include <string.h>
#include <ctype.h>

/* Global configuration for EP seeds */
typedef struct {
    EP_Rules **rules;
    size_t count;
} EP_GlobalConfig;

static EP_GlobalConfig global_config = {NULL, 0};

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

/* EP Rule Management */

void cb_free_ep_rules(EP_Rules *rules) {
    if (!rules) return;
    if (rules->extension) free(rules->extension);
    if (rules->shebang_pattern) free(rules->shebang_pattern);
    for (size_t i = 0; i < rules->keyword_count; i++) free(rules->keywords[i]);
    free(rules->keywords);
    for (size_t i = 0; i < rules->operator_count; i++) free(rules->operators[i]);
    free(rules->operators);
    for (size_t i = 0; i < rules->line_comment_count; i++) free(rules->line_comment_starts[i]);
    free(rules->line_comment_starts);
    for (size_t i = 0; i < rules->block_comment_count; i++) {
        free(rules->block_comment_starts[i]);
        if (rules->block_comment_ends && rules->block_comment_ends[i]) free(rules->block_comment_ends[i]);
    }
    free(rules->block_comment_starts);
    if (rules->block_comment_ends) free(rules->block_comment_ends);
    free(rules->string_quotes);
    free(rules);
}

static int add_unique_string(char ***list, size_t *count, const char *str) {
    if (!str || strlen(str) == 0) return 0;
    for (size_t i = 0; i < *count; i++) {
        if (strcmp((*list)[i], str) == 0) return 0;
    }
    *list = realloc(*list, sizeof(char*) * (*count + 1));
    (*list)[*count] = strdup(str);
    (*count)++;
    return 1;
}

static void add_unique_char(char **list, size_t *count, char c) {
    for (size_t i = 0; i < *count; i++) {
        if ((*list)[i] == c) return;
    }
    *list = realloc(*list, *count + 1);
    (*list)[*count] = c;
    (*count)++;
}

/* Sort operators by length descending to match longest first */
static void sort_operators(EP_Rules *rules) {
    if (!rules || rules->operator_count < 2) return;
    for (size_t i = 0; i < rules->operator_count - 1; i++) {
        for (size_t j = i + 1; j < rules->operator_count; j++) {
            if (strlen(rules->operators[i]) < strlen(rules->operators[j])) {
                char *tmp = rules->operators[i];
                rules->operators[i] = rules->operators[j];
                rules->operators[j] = tmp;
            }
        }
    }
}

static EP_Rules* copy_ep_rules(EP_Rules *src) {
    if (!src) return NULL;
    EP_Rules *dst = calloc(1, sizeof(EP_Rules));
    if (src->extension) dst->extension = strdup(src->extension);
    if (src->shebang_pattern) dst->shebang_pattern = strdup(src->shebang_pattern);
    for (size_t i = 0; i < src->keyword_count; i++) add_unique_string(&dst->keywords, &dst->keyword_count, src->keywords[i]);
    for (size_t i = 0; i < src->operator_count; i++) add_unique_string(&dst->operators, &dst->operator_count, src->operators[i]);
    for (size_t i = 0; i < src->line_comment_count; i++) add_unique_string(&dst->line_comment_starts, &dst->line_comment_count, src->line_comment_starts[i]);
    for (size_t i = 0; i < src->block_comment_count; i++) {
        if (add_unique_string(&dst->block_comment_starts, &dst->block_comment_count, src->block_comment_starts[i])) {
            dst->block_comment_ends = realloc(dst->block_comment_ends, sizeof(char*) * dst->block_comment_count);
            dst->block_comment_ends[dst->block_comment_count - 1] = src->block_comment_ends[i] ? strdup(src->block_comment_ends[i]) : NULL;
        }
    }
    for (size_t i = 0; i < src->string_quote_count; i++) add_unique_char(&dst->string_quotes, &dst->string_quote_count, src->string_quotes[i]);
    dst->is_positional = src->is_positional;
    sort_operators(dst);
    return dst;
}

static char *global_ep_config_string = NULL;

void cb_set_ep_config_string(const char *str) {
    if (global_ep_config_string) free(global_ep_config_string);
    global_ep_config_string = str ? strdup(str) : NULL;
}

const char *cb_get_ep_config_string(void) {
    return global_ep_config_string;
}

void cb_load_ep_config_from_string(const char *config_str) {
    if (!config_str) return;

    char *copy = strdup(config_str);
    char *saveptr;
    char *line = strtok_r(copy, "\n", &saveptr);
    EP_Rules *current = NULL;
    
    while (line) {
        char *l = line;
        while (isspace(*l)) l++;
        if (*l == '#' || *l == '\0') {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        if (*l == '[') {
            char *end = strchr(l, ']');
            if (end) {
                *end = '\0';
                current = calloc(1, sizeof(EP_Rules));
                current->extension = strdup(l + 1);
                global_config.rules = realloc(global_config.rules, sizeof(EP_Rules*) * (global_config.count + 1));
                global_config.rules[global_config.count++] = current;
            }
        } else if (current) {
            char *eq = strchr(l, '=');
            if (eq) {
                *eq = '\0';
                char *val = eq + 1;
                char *vend = val + strlen(val) - 1;
                while (vend > val && isspace(*vend)) { *vend = '\0'; vend--; }

                if (strcmp(l, "keywords") == 0) {
                    char *tok = strtok_r(val, ",", &val);
                    while (tok) { add_unique_string(&current->keywords, &current->keyword_count, tok); tok = strtok_r(NULL, ",", &val); }
                } else if (strcmp(l, "operators") == 0) {
                    char *tok = strtok_r(val, ",", &val);
                    while (tok) { add_unique_string(&current->operators, &current->operator_count, tok); tok = strtok_r(NULL, ",", &val); }
                    sort_operators(current);
                } else if (strcmp(l, "line_comment") == 0) {
                    add_unique_string(&current->line_comment_starts, &current->line_comment_count, val);
                } else if (strcmp(l, "block_start") == 0) {
                    if (add_unique_string(&current->block_comment_starts, &current->block_comment_count, val)) {
                        current->block_comment_ends = realloc(current->block_comment_ends, sizeof(char*) * current->block_comment_count);
                        current->block_comment_ends[current->block_comment_count - 1] = NULL;
                    }
                } else if (strcmp(l, "block_end") == 0) {
                    if (current->block_comment_count > 0) {
                        current->block_comment_ends[current->block_comment_count - 1] = strdup(val);
                    }
                } else if (strcmp(l, "quotes") == 0) {
                    while (*val) { add_unique_char(&current->string_quotes, &current->string_quote_count, *val); val++; }
                } else if (strcmp(l, "positional") == 0) {
                    current->is_positional = atoi(val);
                } else if (strcmp(l, "shebang") == 0) {
                    current->shebang_pattern = strdup(val);
                }
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(copy);
    LOG("cb_load_ep_config_from_string: loaded %zu languages", global_config.count);
}

void cb_load_ep_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        LOG("cb_load_ep_config: could not open %s", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = malloc(fsize + 1);
    size_t read_bytes = fread(string, 1, fsize, f);
    fclose(f);
    string[read_bytes] = '\0';

    cb_load_ep_config_from_string(string);
    cb_set_ep_config_string(string);
    free(string);
}

/* 
 * Lightweight line-based heuristic scan using learned/seeded rules.
 * "Do No Harm": Characters that belong to a parsed node (node != NULL) are NEVER overwritten,
 * but the scanner still reads them to maintain string/comment state.
 */
static void cb_emergency_scan_line(CodeBuffer *cb, int line_idx) {
    if (line_idx < 0 || line_idx >= (int)cb->line_count) return;
    CodeBufferLine *line = &cb->lines[line_idx];
    EP_Rules *rules = cb->ep_rules;
    
    char in_string = 0;
    char string_char = 0;
    char in_comment = 0;

    for (size_t i = 0; i < line->length; i++) {
        CodeBufferCharacter *c = &line->characters[i];
        char32_t cp = c->character[0];

        /* Reset to default for unparsed characters */
        if (c->node == NULL) {
            c->token_type = LEXER_TOKEN;
            c->severity = CB_NONE;
            if (utf32_isspace(cp)) {
                c->token_type = LEXER_WHITESPACE;
            }
        }

        if (!in_comment) {
            if (!in_string) {
                int is_quote = 0;
                if (rules && rules->string_quote_count > 0) {
                    for (size_t q = 0; q < rules->string_quote_count; q++) if (cp == (char32_t)rules->string_quotes[q]) { is_quote = 1; break; }
                }
                if (is_quote) { 
                    in_string = 1; 
                    string_char = (char)cp; 
                    if (c->node == NULL) c->token_type = LEXER_STRING_LITERAL; 
                    continue; 
                }
            } else if (in_string && cp == (char32_t)string_char) { 
                in_string = 0; 
                if (c->node == NULL) c->token_type = LEXER_STRING_LITERAL; 
                continue; 
            }
            if (in_string) { 
                if (c->node == NULL) c->token_type = LEXER_STRING_LITERAL; 
                continue; 
            }
        }

        if (!in_string && !in_comment) {
            int is_comment_start = 0;
            if (rules) {
                /* Line comments */
                for (size_t lc = 0; lc < rules->line_comment_count; lc++) {
                    const char *prefix = rules->line_comment_starts[lc];
                    size_t plen = strlen(prefix);
                    if (i + plen <= line->length) {
                        int match = 1;
                        for (size_t pi = 0; pi < plen; pi++) {
                            if ((char)line->characters[i+pi].character[0] != prefix[pi]) { match = 0; break; }
                        }
                        if (match) { is_comment_start = 1; break; }
                    }
                }
                /* Block comments (line-based heuristic) */
                if (!is_comment_start) {
                    for (size_t bc = 0; bc < rules->block_comment_count; bc++) {
                        const char *prefix = rules->block_comment_starts[bc];
                        size_t plen = strlen(prefix);
                        if (i + plen <= line->length) {
                            int match = 1;
                            for (size_t pi = 0; pi < plen; pi++) {
                                if ((char)line->characters[i+pi].character[0] != prefix[pi]) { match = 0; break; }
                            }
                            if (match) { is_comment_start = 1; break; }
                        }
                    }
                }
            }
            if (is_comment_start) in_comment = 1;
        }
        
        if (in_comment) { 
            if (c->node == NULL) c->token_type = LEXER_COMMENT; 
            continue; 
        }

        /* If this character is parsed, we don't need to try to match keywords/operators on it */
        if (c->node != NULL) continue;

        if (c->token_type == LEXER_WHITESPACE) continue;

        if (!in_string && !in_comment) {
            if (cp >= '0' && cp <= '9') { c->token_type = LEXER_NUMBER_LITERAL; continue; }
        }

        if (!in_string && !in_comment && rules) {
            /* Match operators */
            for (size_t o = 0; o < rules->operator_count; o++) {
                const char *op = rules->operators[o];
                size_t olen = strlen(op);
                if (i + olen <= line->length) {
                    int match = 1;
                    for (size_t oi = 0; oi < olen; oi++) {
                        if ((char)line->characters[i+oi].character[0] != op[oi] || line->characters[i+oi].node != NULL) { match = 0; break; }
                    }
                    if (match) { 
                        for (size_t oi = 0; oi < olen; oi++) { line->characters[i+oi].token_type = LEXER_OPERATOR; } 
                        i += olen - 1; 
                        goto next_char; 
                    }
                }
            }

            /* Match keywords (Whole word boundary check) */
            if (isalpha((char)cp) || cp == '_') {
                int valid_start = (i == 0 || line->characters[i-1].node != NULL || (!isalnum((char)line->characters[i-1].character[0]) && line->characters[i-1].character[0] != '_'));
                if (valid_start) {
                    size_t start_i = i;
                    while (i < line->length && (isalnum((char)line->characters[i].character[0]) || line->characters[i].character[0] == '_')) {
                        if (line->characters[i].node != NULL) break;
                        i++;
                    }
                    int valid_end = (i == line->length || line->characters[i].node != NULL || (!isalnum((char)line->characters[i].character[0]) && line->characters[i].character[0] != '_'));
                    if (valid_end) {
                        size_t len = i - start_i;
                        char *word = malloc(len + 1);
                        for (size_t wi = 0; wi < len; wi++) word[wi] = (char)line->characters[start_i+wi].character[0];
                        word[len] = '\0';
                        int is_kw = 0;
                        for (size_t k = 0; k < rules->keyword_count; k++) {
                            if (strcmp(rules->keywords[k], word) == 0) { is_kw = 1; break; }
                        }
                        if (is_kw) {
                            for (size_t wi = 0; wi < len; wi++) { line->characters[start_i+wi].token_type = LEXER_KEYWORD; }
                        }
                        free(word);
                    }
                    i--; /* backtrack for loop increment */
                    goto next_char;
                } else {
                    while (i < line->length && (isalnum((char)line->characters[i].character[0]) || line->characters[i].character[0] == '_')) {
                        if (line->characters[i].node != NULL) break;
                        i++;
                    }
                    i--;
                    goto next_char;
                }
            }
        }
        next_char:;
    }
}

void cb_seed_ep_rules(CodeBuffer *cb, const char *filename) {
    if (!cb || !filename) return;

    EP_Rules *match = NULL;
    const char *ext = strrchr(filename, '.');

    if (ext) {
        for (size_t i = 0; i < global_config.count; i++) {
            if (strcmp(global_config.rules[i]->extension, ext) == 0) { match = global_config.rules[i]; break; }
        }
    }

    if (!match && cb->line_count > 0) {
        char *first_line = line_to_utf8(&cb->lines[0]);
        if (first_line) {
            if (strncmp(first_line, "#!", 2) == 0) {
                for (size_t i = 0; i < global_config.count; i++) {
                    if (global_config.rules[i]->shebang_pattern && strstr(first_line, global_config.rules[i]->shebang_pattern)) { match = global_config.rules[i]; break; }
                }
            }
            free(first_line);
        }
    }

    if (match) {
        if (cb->ep_rules) cb_free_ep_rules(cb->ep_rules);
        cb->ep_rules = copy_ep_rules(match);
        for (size_t i = 0; i < cb->line_count; i++) cb_emergency_scan_line(cb, (int)i);
    }
}
static void extract_rules_recursive(CodeBuffer *cb, CB_Node *node, EP_Rules *rules) {
    if (!node) return;

    char *text = NULL;
    if (node->type < 70 && node->severity != CB_ERROR) { 
        size_t start = node->pos;
        size_t len = node->length;
        text = malloc(len + 1);
        size_t written = 0;
        size_t current_pos = 0;
        for (size_t l = 0; l < cb->line_count && written < len; l++) {
            size_t line_len = cb->lines[l].length;
            if (current_pos + line_len + 1 > start) {
                size_t offset = (start > current_pos) ? (start - current_pos) : 0;
                for (size_t c = offset; c < line_len && written < len; c++) text[written++] = (char)cb->lines[l].characters[c].character[0];
                if (written < len && current_pos + line_len + 1 > start + written) text[written++] = '\n';
            }
            current_pos += line_len + 1;
        }
        text[written] = '\0';
    }

    if (text) {
        switch (node->type) {
            case LEXER_KEYWORD: add_unique_string(&rules->keywords, &rules->keyword_count, text); break;
            case LEXER_OPERATOR:
            case LEXER_OPERATOR_ASSIGN:
            case LEXER_OPERATOR_ARITHMETIC:
            case LEXER_OPERATOR_LOGICAL: add_unique_string(&rules->operators, &rules->operator_count, text); break;
            case LEXER_STRING_LITERAL: if (strlen(text) > 0) add_unique_char(&rules->string_quotes, &rules->string_quote_count, text[0]); break;
            case LEXER_COMMENT:
                if (strlen(text) >= 2) {
                    if (text[0] == '/' && text[1] == '*') {
                        if (add_unique_string(&rules->block_comment_starts, &rules->block_comment_count, "/*")) {
                            rules->block_comment_ends = realloc(rules->block_comment_ends, sizeof(char*) * rules->block_comment_count);
                            rules->block_comment_ends[rules->block_comment_count - 1] = strdup("*/");
                        }
                    } else if (text[0] == '<' && text[1] == '!' && text[2] == '-' && text[3] == '-') {
                        if (add_unique_string(&rules->block_comment_starts, &rules->block_comment_count, "<!--")) {
                            rules->block_comment_ends = realloc(rules->block_comment_ends, sizeof(char*) * rules->block_comment_count);
                            rules->block_comment_ends[rules->block_comment_count - 1] = strdup("-->");
                        }
                    } else if (!strstr(text, "\n")) {
                        /* Line comments */
                        if (text[0] == '/' && text[1] == '/') add_unique_string(&rules->line_comment_starts, &rules->line_comment_count, "//");
                        else if (text[0] == '#') add_unique_string(&rules->line_comment_starts, &rules->line_comment_count, "#");
                        else if (text[0] == '-' && text[1] == '-') add_unique_string(&rules->line_comment_starts, &rules->line_comment_count, "--");
                    }
                } else if (!strstr(text, "\n") && strlen(text) == 1) {
                    if (text[0] == '#') add_unique_string(&rules->line_comment_starts, &rules->line_comment_count, "#");
                }
                break;
            default: break;
        }
        free(text);
    }

    CB_Node *child = node->child;
    while (child) { extract_rules_recursive(cb, child, rules); child = child->sibling; }
}

void cb_learn_ep_rules(CodeBuffer *cb) {
    if (!cb || !cb->parse_tree || !cb->parse_tree->root) return;
    if (!cb->ep_rules) cb->ep_rules = calloc(1, sizeof(EP_Rules));
    extract_rules_recursive(cb, cb->parse_tree->root, cb->ep_rules);
    sort_operators(cb->ep_rules);
}

void cb_emergency_parse_transaction(CodeBuffer *cb, Transaction transaction) {
    if (!cb) return;

    if (cb->parse_tree && cb->parse_tree->root) {
        size_t abs_pos = get_abs_pos(cb, transaction.pos_line, transaction.pos_col);
        int shift = 0;
        size_t content_len = transaction.content ? strlen(transaction.content) : 0;
        switch (transaction.type) {
            case TRANSACTION_ADDCHARS: shift = (int)content_len; break;
            case TRANSACTION_DELETECHARS: shift = -transaction.count; break;
            case TRANSACTION_ADDLINE: shift = (int)content_len + 1; break;
            case TRANSACTION_DELETELINE: break;
            default: break;
        }
        if (shift != 0) shift_nodes(cb->parse_tree->root, abs_pos, shift);
    }

    cb_emergency_scan_line(cb, transaction.pos_line);
}
