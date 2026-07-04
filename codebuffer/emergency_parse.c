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
    if (rules->ident_extra_chars) free(rules->ident_extra_chars);
    for (size_t i = 0; i < rules->typed_prefix_rule_count; i++) {
        free(rules->typed_prefix_rules[i].prefix);
    }
    free(rules->typed_prefix_rules);
    for (size_t i = 0; i < rules->typed_span_rule_count; i++) {
        free(rules->typed_span_rules[i].start);
        free(rules->typed_span_rules[i].end);
    }
    free(rules->typed_span_rules);
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

static char *trim_in_place(char *value) {
    char *end;

    while (value && isspace((unsigned char)*value)) value++;
    if (!value || *value == '\0') return value;
    end = value + strlen(value) - 1;
    while (end > value && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return value;
}

static int token_type_from_name(const char *name, char *token_type) {
    char lowered[64];
    size_t len;

    if (!name || !token_type) return 0;
    while (isspace((unsigned char)*name)) name++;
    len = strlen(name);
    while (len > 0 && isspace((unsigned char)name[len - 1])) len--;
    if (len == 0 || len >= sizeof(lowered)) return 0;
    for (size_t i = 0; i < len; i++) lowered[i] = (char)tolower((unsigned char)name[i]);
    lowered[len] = '\0';

    if (strcmp(lowered, "preprocessor") == 0 || strcmp(lowered, "lexer_preprocessor") == 0) {
        *token_type = (char)LEXER_PREPROCESSOR;
    } else if (strcmp(lowered, "macro") == 0 || strcmp(lowered, "macro_identifier") == 0 ||
               strcmp(lowered, "macro_name") == 0 || strcmp(lowered, "lexer_macro_identifier") == 0) {
        *token_type = (char)LEXER_MACRO_IDENTIFIER;
    } else if (strcmp(lowered, "macro_variable") == 0 || strcmp(lowered, "macro_var") == 0 ||
               strcmp(lowered, "template_variable") == 0 || strcmp(lowered, "lexer_macro_variable") == 0) {
        *token_type = (char)LEXER_MACRO_VARIABLE;
    } else if (strcmp(lowered, "macro_constant") == 0 || strcmp(lowered, "preprocessor_constant") == 0 ||
               strcmp(lowered, "lexer_macro_constant") == 0) {
        *token_type = (char)LEXER_MACRO_CONSTANT;
    } else if (strcmp(lowered, "constant") == 0 || strcmp(lowered, "constant_identifier") == 0 ||
               strcmp(lowered, "lexer_constant_identifier") == 0) {
        *token_type = (char)LEXER_CONSTANT_IDENTIFIER;
    } else if (strcmp(lowered, "function") == 0 || strcmp(lowered, "function_identifier") == 0 ||
               strcmp(lowered, "lexer_function_identifier") == 0) {
        *token_type = (char)LEXER_FUNCTION_IDENTIFIER;
    } else if (strcmp(lowered, "identifier") == 0 || strcmp(lowered, "lexer_identifier") == 0) {
        *token_type = (char)LEXER_IDENTIFIER;
    } else if (strcmp(lowered, "keyword") == 0 || strcmp(lowered, "lexer_keyword") == 0) {
        *token_type = (char)LEXER_KEYWORD;
    } else if (strcmp(lowered, "operator") == 0 || strcmp(lowered, "lexer_operator") == 0) {
        *token_type = (char)LEXER_OPERATOR;
    } else if (strcmp(lowered, "separator") == 0 || strcmp(lowered, "lexer_separator") == 0) {
        *token_type = (char)LEXER_SEPARATOR;
    } else if (strcmp(lowered, "string") == 0 || strcmp(lowered, "string_literal") == 0 ||
               strcmp(lowered, "lexer_string_literal") == 0) {
        *token_type = (char)LEXER_STRING_LITERAL;
    } else if (strcmp(lowered, "comment") == 0 || strcmp(lowered, "lexer_comment") == 0) {
        *token_type = (char)LEXER_COMMENT;
    } else if (strcmp(lowered, "number") == 0 || strcmp(lowered, "number_literal") == 0 ||
               strcmp(lowered, "lexer_number_literal") == 0) {
        *token_type = (char)LEXER_NUMBER_LITERAL;
    } else {
        return 0;
    }
    return 1;
}

static void add_typed_prefix_rule(EP_Rules *rules, const char *prefix, char token_type) {
    EP_TypedPrefixRule *new_rules;

    if (!rules || !prefix || prefix[0] == '\0') return;
    for (size_t i = 0; i < rules->typed_prefix_rule_count; i++) {
        if (strcmp(rules->typed_prefix_rules[i].prefix, prefix) == 0 &&
            rules->typed_prefix_rules[i].token_type == token_type) {
            return;
        }
    }

    new_rules = realloc(rules->typed_prefix_rules,
                        sizeof(EP_TypedPrefixRule) * (rules->typed_prefix_rule_count + 1));
    if (!new_rules) return;
    rules->typed_prefix_rules = new_rules;
    rules->typed_prefix_rules[rules->typed_prefix_rule_count].prefix = strdup(prefix);
    rules->typed_prefix_rules[rules->typed_prefix_rule_count].token_type = token_type;
    rules->typed_prefix_rule_count++;
}

static void add_typed_span_rule(EP_Rules *rules, const char *start, const char *end, char token_type) {
    EP_TypedSpanRule *new_rules;

    if (!rules || !start || !end || start[0] == '\0' || end[0] == '\0') return;
    for (size_t i = 0; i < rules->typed_span_rule_count; i++) {
        if (strcmp(rules->typed_span_rules[i].start, start) == 0 &&
            strcmp(rules->typed_span_rules[i].end, end) == 0 &&
            rules->typed_span_rules[i].token_type == token_type) {
            return;
        }
    }

    new_rules = realloc(rules->typed_span_rules,
                        sizeof(EP_TypedSpanRule) * (rules->typed_span_rule_count + 1));
    if (!new_rules) return;
    rules->typed_span_rules = new_rules;
    rules->typed_span_rules[rules->typed_span_rule_count].start = strdup(start);
    rules->typed_span_rules[rules->typed_span_rule_count].end = strdup(end);
    rules->typed_span_rules[rules->typed_span_rule_count].token_type = token_type;
    rules->typed_span_rule_count++;
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
    if (src->ident_extra_chars) dst->ident_extra_chars = strdup(src->ident_extra_chars);
    for (size_t i = 0; i < src->typed_prefix_rule_count; i++) {
        add_typed_prefix_rule(dst, src->typed_prefix_rules[i].prefix, src->typed_prefix_rules[i].token_type);
    }
    for (size_t i = 0; i < src->typed_span_rule_count; i++) {
        add_typed_span_rule(dst,
                            src->typed_span_rules[i].start,
                            src->typed_span_rules[i].end,
                            src->typed_span_rules[i].token_type);
    }
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

static void parse_comma_list(char ***list, size_t *count, char *val) {
    char *start = val;
    char *curr = val;
    while (*curr) {
        if (*curr == '\\' && *(curr + 1) == ',') {
            memmove(curr, curr + 1, strlen(curr));
            curr++;
        } else if (*curr == ',') {
            *curr = '\0';
            add_unique_string(list, count, start);
            start = curr + 1;
            curr++;
        } else {
            curr++;
        }
    }
    if (*start) {
        add_unique_string(list, count, start);
    }
}

static void parse_typed_prefix_list(EP_Rules *rules, char *val) {
    char **items = NULL;
    size_t item_count = 0;

    parse_comma_list(&items, &item_count, val);
    for (size_t i = 0; i < item_count; i++) {
        char *sep = strrchr(items[i], ':');
        char token_type;
        if (sep) {
            *sep = '\0';
            char *prefix = trim_in_place(items[i]);
            char *type_name = trim_in_place(sep + 1);
            if (token_type_from_name(type_name, &token_type)) {
                add_typed_prefix_rule(rules, prefix, token_type);
            }
        }
        free(items[i]);
    }
    free(items);
}

static void parse_typed_span_list(EP_Rules *rules, char *val) {
    char **items = NULL;
    size_t item_count = 0;

    parse_comma_list(&items, &item_count, val);
    for (size_t i = 0; i < item_count; i++) {
        char *first_sep = strchr(items[i], ':');
        char *last_sep = strrchr(items[i], ':');
        char token_type;
        if (first_sep && last_sep && first_sep != last_sep) {
            *first_sep = '\0';
            *last_sep = '\0';
            char *start = trim_in_place(items[i]);
            char *end = trim_in_place(first_sep + 1);
            char *type_name = trim_in_place(last_sep + 1);
            if (token_type_from_name(type_name, &token_type)) {
                add_typed_span_rule(rules, start, end, token_type);
            }
        }
        free(items[i]);
    }
    free(items);
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
                    parse_comma_list(&current->keywords, &current->keyword_count, val);
                } else if (strcmp(l, "operators") == 0) {
                    parse_comma_list(&current->operators, &current->operator_count, val);
                    sort_operators(current);
                } else if (strcmp(l, "line_comment") == 0) {
                    parse_comma_list(&current->line_comment_starts, &current->line_comment_count, val);
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
                } else if (strcmp(l, "ident_extra_chars") == 0) {
                    current->ident_extra_chars = strdup(val);
                } else if (strcmp(l, "prefix_tokens") == 0) {
                    parse_typed_prefix_list(current, val);
                } else if (strcmp(l, "span_tokens") == 0) {
                    parse_typed_span_list(current, val);
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
static int line_matches_ascii(CodeBufferLine *line, size_t pos, const char *text, int require_unparsed) {
    size_t len;

    if (!line || !text) return 0;
    len = strlen(text);
    if (len == 0 || pos + len > line->length) return 0;
    for (size_t i = 0; i < len; i++) {
        if (require_unparsed && line->characters[pos + i].node != NULL) return 0;
        if ((char)line->characters[pos + i].character[0] != text[i]) return 0;
    }
    return 1;
}

static int is_line_prefix_position(CodeBufferLine *line, size_t pos) {
    if (!line) return 0;
    for (size_t i = 0; i < pos; i++) {
        if (!utf32_isspace(line->characters[i].character[0])) return 0;
    }
    return 1;
}

static void color_unparsed_range(CodeBufferLine *line, size_t start, size_t end, char token_type) {
    if (!line || start >= end || start >= line->length) return;
    if (end > line->length) end = line->length;
    for (size_t i = start; i < end; i++) {
        if (line->characters[i].node == NULL) {
            line->characters[i].token_type = token_type;
        }
    }
}

static int apply_typed_span_rule(CodeBufferLine *line, size_t pos, EP_Rules *rules, size_t *end_pos) {
    if (!line || !rules || !end_pos) return 0;

    for (size_t r = 0; r < rules->typed_span_rule_count; r++) {
        EP_TypedSpanRule *rule = &rules->typed_span_rules[r];
        size_t start_len = strlen(rule->start);
        size_t end_len = strlen(rule->end);

        if (!line_matches_ascii(line, pos, rule->start, 1)) continue;

        for (size_t cursor = pos + start_len; cursor + end_len <= line->length; cursor++) {
            if (line->characters[cursor].node != NULL) break;
            if (line_matches_ascii(line, cursor, rule->end, 1)) {
                size_t span_end = cursor + end_len;
                color_unparsed_range(line, pos, span_end, rule->token_type);
                *end_pos = span_end;
                return 1;
            }
        }
    }
    return 0;
}

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

        if (!in_string && !in_comment && rules) {
            for (size_t r = 0; r < rules->typed_prefix_rule_count; r++) {
                EP_TypedPrefixRule *rule = &rules->typed_prefix_rules[r];
                if (is_line_prefix_position(line, i) && line_matches_ascii(line, i, rule->prefix, 1)) {
                    color_unparsed_range(line, i, line->length, rule->token_type);
                    i = line->length - 1;
                    goto next_char;
                }
            }

            size_t span_end = 0;
            if (apply_typed_span_rule(line, i, rules, &span_end)) {
                i = span_end - 1;
                goto next_char;
            }
        }

        if (!in_string && !in_comment) {
            if (cp >= '0' && cp <= '9') {
                while (i < line->length) {
                    char next_cp = (char)line->characters[i].character[0];
                    if (line->characters[i].node != NULL) break;
                    if (isalnum(next_cp) || next_cp == '.' || next_cp == '_') {
                        line->characters[i].token_type = LEXER_NUMBER_LITERAL;
                        i++;
                    } else {
                        break;
                    }
                }
                i--;
                goto next_char;
            }
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

            /* Match keywords and identifiers (Whole word boundary check) */
            int is_ident_char = isalpha((char)cp) || cp == '_';
            if (!is_ident_char && rules->ident_extra_chars && strchr(rules->ident_extra_chars, (char)cp)) is_ident_char = 1;

            if (is_ident_char) {
                int prev_is_ident = 0;
                if (i > 0 && line->characters[i-1].node == NULL) {
                    char prev_cp = (char)line->characters[i-1].character[0];
                    if (isalnum(prev_cp) || prev_cp == '_') prev_is_ident = 1;
                    else if (rules->ident_extra_chars && strchr(rules->ident_extra_chars, prev_cp)) prev_is_ident = 1;
                }
                
                if (!prev_is_ident) {
                    size_t start_i = i;
                    while (i < line->length) {
                        if (line->characters[i].node != NULL) break;
                        char next_cp = (char)line->characters[i].character[0];
                        int next_is_ident = isalnum(next_cp) || next_cp == '_';
                        if (!next_is_ident && rules->ident_extra_chars && strchr(rules->ident_extra_chars, next_cp)) next_is_ident = 1;
                        if (!next_is_ident) break;
                        i++;
                    }
                    
                    size_t len = i - start_i;
                    char *word = malloc(len + 1);
                    for (size_t wi = 0; wi < len; wi++) word[wi] = (char)line->characters[start_i+wi].character[0];
                    word[len] = '\0';
                    
                    int is_kw = 0;
                    for (size_t k = 0; k < rules->keyword_count; k++) {
                        if (strcmp(rules->keywords[k], word) == 0) { is_kw = 1; break; }
                    }
                    
                    for (size_t wi = 0; wi < len; wi++) { 
                        line->characters[start_i+wi].token_type = is_kw ? LEXER_KEYWORD : LEXER_IDENTIFIER; 
                    }
                    
                    free(word);
                    i--; /* backtrack for loop increment */
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
            case LEXER_PREPROCESSOR: {
                char *prefix = trim_in_place(text);
                size_t prefix_len = 0;
                if (prefix[0] == '#' || prefix[0] == '%') {
                    while (prefix[prefix_len] == prefix[0]) prefix_len++;
                    if (prefix_len > 0) {
                        char saved = prefix[prefix_len];
                        prefix[prefix_len] = '\0';
                        add_typed_prefix_rule(rules, prefix, (char)LEXER_PREPROCESSOR);
                        prefix[prefix_len] = saved;
                    }
                }
                break;
            }
            case LEXER_MACRO_VARIABLE: {
                size_t len = strlen(text);
                if (len >= 2 && text[0] == '{' && text[len - 1] == '}') {
                    add_typed_span_rule(rules, "{", "}", (char)LEXER_MACRO_VARIABLE);
                }
                break;
            }
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
