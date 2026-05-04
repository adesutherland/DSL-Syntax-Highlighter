#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "dslsyntax_log.h"
#include "tree_sitter_adapter.h"

typedef struct SourceMap {
    const char *source;
    size_t byte_length;
    size_t char_count;
    size_t *char_to_byte;
} SourceMap;

static int utf8_width(unsigned char ch) {
    if ((ch & 0x80) == 0) return 1;
    if ((ch & 0xE0) == 0xC0) return 2;
    if ((ch & 0xF0) == 0xE0) return 3;
    if ((ch & 0xF8) == 0xF0) return 4;
    return 1;
}

static int append_size(size_t **values, size_t *count, size_t value) {
    size_t *new_values = (size_t*)safe_realloc(*values, (*count + 1) * sizeof(size_t));
    if (!new_values) return -1;
    *values = new_values;
    (*values)[(*count)++] = value;
    return 0;
}

static int source_map_init(SourceMap *map, const char *source) {
    size_t byte_pos = 0;

    memset(map, 0, sizeof(*map));
    map->source = source ? source : "";
    map->byte_length = strlen(map->source);

    while (byte_pos < map->byte_length) {
        int width;

        if (append_size(&map->char_to_byte, &map->char_count, byte_pos) != 0) return -1;
        width = utf8_width((unsigned char)map->source[byte_pos]);
        if (byte_pos + (size_t)width > map->byte_length) width = 1;
        byte_pos += (size_t)width;
    }

    if (append_size(&map->char_to_byte, &map->char_count, map->byte_length) != 0) return -1;
    map->char_count--;
    return 0;
}

static void source_map_free(SourceMap *map) {
    free(map->char_to_byte);
}

static size_t byte_to_char(const SourceMap *map, size_t byte_pos) {
    size_t lo = 0;
    size_t hi = map->char_count;

    if (byte_pos >= map->byte_length) return map->char_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (map->char_to_byte[mid] < byte_pos) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static int str_eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static int str_has_prefix(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static int str_contains(const char *text, const char *needle) {
    return strstr(text, needle) != NULL;
}

static int is_one_of(const char *type, const char *const *values, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (str_eq(type, values[i])) return 1;
    }
    return 0;
}

static int is_c_keyword(const char *type) {
    static const char *const keywords[] = {
        "auto", "break", "case", "const", "continue", "default", "do", "else", "extern",
        "for", "goto", "if", "inline", "register", "restrict", "return", "sizeof", "static",
        "switch", "typedef", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool",
        "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
    };
    return is_one_of(type, keywords, sizeof(keywords) / sizeof(keywords[0]));
}

static int is_python_keyword(const char *type) {
    static const char *const keywords[] = {
        "and", "as", "assert", "async", "await", "break", "case", "class", "continue", "def",
        "del", "elif", "else", "except", "False", "finally", "for", "from", "global", "if",
        "import", "in", "is", "lambda", "match", "None", "nonlocal", "not", "or", "pass",
        "raise", "return", "True", "try", "while", "with", "yield"
    };
    return is_one_of(type, keywords, sizeof(keywords) / sizeof(keywords[0]));
}

static int is_javascript_keyword(const char *type) {
    static const char *const keywords[] = {
        "async", "await", "break", "case", "catch", "class", "const", "continue", "debugger",
        "default", "delete", "do", "else", "export", "extends", "finally", "for", "from",
        "function", "get", "if", "import", "in", "instanceof", "let", "new", "of", "return",
        "set", "static", "super", "switch", "target", "this", "throw", "try", "typeof", "var",
        "void", "while", "with", "yield", "true", "false", "null", "undefined"
    };
    return is_one_of(type, keywords, sizeof(keywords) / sizeof(keywords[0]));
}

static int is_keyword(const TSAdapterProfile *profile, const char *type) {
    switch (profile->language) {
        case TS_ADAPTER_C: return is_c_keyword(type);
        case TS_ADAPTER_PYTHON: return is_python_keyword(type);
        case TS_ADAPTER_JAVASCRIPT: return is_javascript_keyword(type);
        default: return 0;
    }
}

static int is_assignment_operator(const char *type) {
    static const char *const ops[] = {
        "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "**=", "//=",
        "=>"
    };
    return is_one_of(type, ops, sizeof(ops) / sizeof(ops[0]));
}

static int is_arithmetic_operator(const char *type) {
    static const char *const ops[] = {"+", "-", "*", "/", "%", "**", "//", "++", "--"};
    return is_one_of(type, ops, sizeof(ops) / sizeof(ops[0]));
}

static int is_logical_operator(const char *type) {
    static const char *const ops[] = {"&&", "||", "!", "and", "or", "not"};
    return is_one_of(type, ops, sizeof(ops) / sizeof(ops[0]));
}

static CB_NodeType map_leaf_type(TSNode node, const TSAdapterProfile *profile) {
    const char *type = ts_node_type(node);
    TSNode parent = ts_node_parent(node);
    const char *parent_type = ts_node_is_null(parent) ? "" : ts_node_type(parent);

    if (ts_node_is_missing(node) || str_eq(type, "ERROR")) return LEXER_UNKNOWN;
    if (str_eq(type, "comment")) return LEXER_COMMENT;
    if (str_has_prefix(type, "preproc") || type[0] == '#') return LEXER_PREPROCESSOR;
    if (str_contains(type, "string") || str_contains(type, "char_literal") ||
        str_eq(type, "escape_sequence") || str_eq(type, "regex")) {
        return LEXER_STRING_LITERAL;
    }
    if (str_contains(type, "number") || str_contains(type, "integer") || str_contains(type, "float")) {
        return LEXER_NUMBER_LITERAL;
    }
    if (is_keyword(profile, type)) return LEXER_KEYWORD;
    if (is_assignment_operator(type)) return LEXER_OPERATOR_ASSIGN;
    if (is_arithmetic_operator(type)) return LEXER_OPERATOR_ARITHMETIC;
    if (is_logical_operator(type)) return LEXER_OPERATOR_LOGICAL;
    if (str_eq(type, "{")) return LEXER_LH_CODEBLOCK;
    if (str_eq(type, "}")) return LEXER_RH_CODEBLOCK;
    if (str_eq(type, "(") || str_eq(type, "[")) return LEXER_LH_EXPR;
    if (str_eq(type, ")") || str_eq(type, "]")) return LEXER_RH_EXPR;
    if (str_eq(type, ";")) return LEXER_STATEMENT_SEPARATOR;
    if (str_eq(type, ",") || str_eq(type, ".") || str_eq(type, ":")) return LEXER_SEPARATOR;
    if (strlen(type) <= 3 && ispunct((unsigned char)type[0])) return LEXER_OPERATOR;

    if (str_eq(type, "primitive_type") || str_eq(type, "type_identifier") || str_eq(type, "sized_type_specifier")) {
        return LEXER_TYPE_IDENTIFIER;
    }
    if (str_eq(type, "constant") || str_eq(type, "true") || str_eq(type, "false") || str_eq(type, "null") ||
        str_eq(type, "none")) {
        return LEXER_CONSTANT_IDENTIFIER;
    }
    if (str_eq(type, "identifier") || str_eq(type, "property_identifier") || str_eq(type, "field_identifier")) {
        if (str_contains(parent_type, "call") || str_contains(parent_type, "function_declarator") ||
            str_contains(parent_type, "function_declaration") || str_contains(parent_type, "function_definition") ||
            str_contains(parent_type, "method")) {
            return LEXER_FUNCTION_IDENTIFIER;
        }
        return LEXER_IDENTIFIER;
    }

    return LEXER_TOKEN;
}

static CB_NodeType map_structural_type(TSNode node) {
    const char *type = ts_node_type(node);

    if (str_contains(type, "function") || str_contains(type, "method")) return PARSE_TREE_FUNCTION;
    if (str_contains(type, "compound_statement") || str_contains(type, "block")) return PARSE_TREE_CODEBLOCK;
    if (str_contains(type, "class") || str_contains(type, "struct") || str_contains(type, "enum") ||
        str_contains(type, "union") || str_contains(type, "interface")) {
        return PARSE_TREE_STRUCTURE;
    }
    if (str_contains(type, "expression") || str_contains(type, "argument") || str_contains(type, "parameter") ||
        str_contains(type, "subscript") || str_contains(type, "call")) {
        return PARSE_TREE_EXPR;
    }
    if (str_contains(type, "declaration") || str_contains(type, "statement") || str_contains(type, "clause") ||
        str_contains(type, "import") || str_contains(type, "export")) {
        return PARSE_TREE_STATEMENT;
    }
    return PARSE_TREE;
}

static void add_ts_node(CB_ParseTree *tb, TSNode node, const SourceMap *map, const TSAdapterProfile *profile) {
    uint32_t child_count = ts_node_child_count(node);
    size_t start = byte_to_char(map, ts_node_start_byte(node));
    size_t end = byte_to_char(map, ts_node_end_byte(node));

    if (end <= start) return;

    if (child_count == 0) {
        CB_Node leaf = cb_create_node(map_leaf_type(node, profile), start, end - start);
        if (ts_node_has_error(node) || ts_node_is_missing(node)) {
            leaf.severity = CB_ERROR;
            leaf.message_code = strdup("TS_PARSE");
            leaf.message = strdup("Tree-sitter parse error");
        }
        cb_add_child_node(tb, leaf);
        return;
    }

    CB_Node tree_node = cb_create_node(map_structural_type(node), start, end - start);
    if (ts_node_has_error(node)) {
        tree_node.severity = CB_ERROR;
        tree_node.message_code = strdup("TS_PARSE");
        tree_node.message = strdup("Tree-sitter parse error");
    }
    cb_add_child_node(tb, tree_node);
    cb_set_current_parent_to_last_node(tb);

    for (uint32_t i = 0; i < child_count; i++) {
        add_ts_node(tb, ts_node_child(node, i), map, profile);
    }

    cb_set_current_parent_to_grandparent(tb);
}

void dslsh_tree_sitter_parse(CodeBuffer *codeBuffer, const TSLanguage *language, const TSAdapterProfile *profile) {
    char *source;
    SourceMap map;
    TSParser *parser;
    TSTree *tree;
    TSNode root;
    CB_ParseTree *tb;
    size_t total_length;

    if (!codeBuffer || !language || !profile) return;

    source = get_code_buffer_source(codeBuffer);
    if (!source) source = strdup("");

    if (source_map_init(&map, source) != 0) {
        source_map_free(&map);
        free(source);
        codeBuffer->parse_tree = NULL;
        return;
    }

    parser = ts_parser_new();
    if (!parser || !ts_parser_set_language(parser, language)) {
        if (parser) ts_parser_delete(parser);
        source_map_free(&map);
        free(source);
        codeBuffer->parse_tree = NULL;
        return;
    }

    tree = ts_parser_parse_string(parser, NULL, source, (uint32_t)strlen(source));
    total_length = get_code_buffer_length(codeBuffer);
    if (total_length > map.char_count) total_length = map.char_count;

    tb = cb_create_token_buffer();
    cb_add_child_node(tb, cb_create_node(PARSE_TREE_FILE, 0, total_length));
    cb_set_current_parent_to_root_node(tb);

    if (tree) {
        root = ts_tree_root_node(tree);
        for (uint32_t i = 0; i < ts_node_child_count(root); i++) {
            add_ts_node(tb, ts_node_child(root, i), &map, profile);
        }
    }

    cb_order_tree(tb);
    cb_add_missing_tokens(tb, codeBuffer, NULL, NULL);
    cb_tweak_tree_positions(tb);
    cb_validate_tree(tb);
    codeBuffer->parse_tree = tb;

    if (tree) ts_tree_delete(tree);
    ts_parser_delete(parser);
    source_map_free(&map);
    free(source);
}
