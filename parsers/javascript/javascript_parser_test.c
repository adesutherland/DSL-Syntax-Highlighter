#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dslsyntax_common.h"
#include "javascript_parser.h"

static InitialLoad *create_initial_load(const char *document_id, const char *source) {
    InitialLoad *load = (InitialLoad*)calloc(1, sizeof(InitialLoad));
    size_t start = 0;
    size_t length = strlen(source);

    load->unique_document_id = strdup(document_id);
    load->change_version = 1;

    for (size_t i = 0; i <= length; i++) {
        if (source[i] == '\0' || source[i] == '\n') {
            size_t line_len = i - start;
            char *line = (char*)malloc(line_len + 1);
            CodeBufferLine *lines;

            if (source[i] == '\0' && i == start && i > 0 && source[i - 1] == '\n') break;
            memcpy(line, source + start, line_len);
            line[line_len] = '\0';

            lines = (CodeBufferLine*)safe_realloc(load->lines, (load->line_count + 1) * sizeof(CodeBufferLine));
            assert(lines != NULL);
            load->lines = lines;
            utf8_to_line(line, &load->lines[load->line_count++]);
            free(line);
            start = i + 1;
        }
    }

    return load;
}

static int count_nodes_of_type(CB_Node *node, CB_NodeType type) {
    int count = 0;
    if (!node) return 0;
    if (node->type == type) count++;
    for (CB_Node *child = node->child; child; child = child->sibling) {
        count += count_nodes_of_type(child, type);
    }
    return count;
}

static CB_Node *find_leaf_at(CB_Node *node, size_t pos) {
    if (!node) return NULL;
    if (!node->child && node->pos <= pos && pos < node->pos + node->length) return node;
    for (CB_Node *child = node->child; child; child = child->sibling) {
        CB_Node *found = find_leaf_at(child, pos);
        if (found) return found;
    }
    return NULL;
}

static void test_javascript_parser_tree_and_tokens(void) {
    const char *source =
        "import { readFile } from \"fs\";\n"
        "function greet(name) {\n"
        "  // hello\n"
        "  const value = 42;\n"
        "  return `hello ${name}`;\n"
        "}\n";
    CodeBuffer *cb = create_code_buffer(NULL, javascript_parser);
    InitialLoad *load = create_initial_load("test.js", source);

    base_load_initial_content(cb, load);

    assert(cb->parse_tree != NULL);
    assert(cb->parse_tree->root != NULL);
    assert(cb->parse_tree->root->type == PARSE_TREE_FILE);
    assert(cb->parse_tree->root->length == get_code_buffer_length(cb));
    assert(count_nodes_of_type(cb->parse_tree->root, PARSE_TREE_FUNCTION) >= 1);
    assert(count_nodes_of_type(cb->parse_tree->root, PARSE_TREE_CODEBLOCK) >= 1);
    assert(count_nodes_of_type(cb->parse_tree->root, LEXER_COMMENT) >= 1);
    assert(count_nodes_of_type(cb->parse_tree->root, LEXER_STRING_LITERAL) >= 1);
    assert(count_nodes_of_type(cb->parse_tree->root, LEXER_NUMBER_LITERAL) >= 1);
    assert(count_nodes_of_type(cb->parse_tree->root, LEXER_KEYWORD) >= 1);
    assert(count_nodes_of_type(cb->parse_tree->root, LEXER_FUNCTION_IDENTIFIER) >= 1);

    CB_Node *first = find_leaf_at(cb->parse_tree->root, 0);
    assert(first != NULL);
    assert(first->type == LEXER_KEYWORD);

    free_code_buffer(cb);
}

int main(void) {
    printf("Testing JavaScript parser...\n");
    test_javascript_parser_tree_and_tokens();
    printf("JavaScript parser tests passed!\n");
    return 0;
}
