#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "dslsyntax_common.h"
#include "serialization.h"

void test_tree_flatten_reconstruct() {
    printf("Testing tree flatten/reconstruct...\n");
    CB_ParseTree *tb = cb_create_token_buffer();
    
    /* Create a sample tree:
       File
         Statement
           Keyword (int)
           Identifier (x)
           Operator (=)
           Number (5)
           Separator (;)
    */
    CB_Node root = cb_create_node(PARSE_TREE_FILE, 0, 10);
    cb_add_child_node(tb, root);
    cb_set_current_parent_to_root_node(tb);

    CB_Node stmt = cb_create_node(PARSE_TREE_STATEMENT, 0, 10);
    cb_add_child_node(tb, stmt);
    cb_set_current_parent_to_last_node(tb);

    cb_add_child_node(tb, cb_create_node(LEXER_KEYWORD, 0, 3));
    cb_add_child_node(tb, cb_create_node(LEXER_WHITESPACE, 3, 1));
    cb_add_child_node(tb, cb_create_node(LEXER_IDENTIFIER, 4, 1));
    cb_add_child_node(tb, cb_create_node(LEXER_OPERATOR_ASSIGN, 6, 1));
    cb_add_child_node(tb, cb_create_node(LEXER_NUMBER_LITERAL, 8, 1));
    cb_add_child_node(tb, cb_create_node(LEXER_STATEMENT_SEPARATOR, 9, 1));

    /* Flatten */
    CB_TokenStream *stream = cb_flatten_tree(tb);
    assert(stream != NULL);
    /* root (DOWN + type) + stmt (DOWN + type) + 6 leaves + stmt (UP) + root (UP) = 12 tokens */
    /* Wait, my flatten adds TREE_DOWN then the type as another token if it has children.
       Let's re-verify logic:
       cb_flatten_node(root)
         adds TREE_DOWN (root.pos, root.len)
         adds PARSE_TREE_FILE (root.pos, root.len)
         recurse(stmt)
            adds TREE_DOWN (stmt.pos, stmt.len)
            adds PARSE_TREE_STATEMENT (stmt.pos, stmt.len)
            recurse(6 leaves) -> adds 6 tokens
            adds TREE_UP
         adds TREE_UP
       Total: 2 + 2 + 6 + 1 + 1 = 12 tokens.
    */
    printf("Flattened stream count: %zu\n", stream->count);
    assert(stream->count == 12);

    /* Reconstruct */
    CB_ParseTree *tb2 = cb_reconstruct_tree(stream);
    assert(tb2 != NULL);
    assert(tb2->root != NULL);
    assert(tb2->root->type == PARSE_TREE_FILE);
    assert(tb2->root->child != NULL);
    assert(tb2->root->child->type == PARSE_TREE_STATEMENT);
    assert(tb2->root->child->child != NULL);
    assert(tb2->root->child->child->type == LEXER_KEYWORD);

    printf("Tree reconstruction successful.\n");

    /* Serialize Token Stream */
    char *serialized = cb_serialize_token_stream(stream);
    printf("Serialized stream length: %zu\n", strlen(serialized));
    
    CB_TokenStream *stream2 = cb_deserialize_token_stream(serialized);
    assert(stream2 != NULL);
    assert(stream2->count == stream->count);
    assert(stream2->tokens[0].type == stream->tokens[0].type);
    
    printf("Token stream serialization successful.\n");

    cb_free_token_buffer(tb);
    cb_free_token_buffer(tb2);
    cb_free_token_stream(stream);
    cb_free_token_stream(stream2);
    free(serialized);
}

void test_delta_serialization() {
    printf("Testing delta serialization...\n");
    Delta delta;
    memset(&delta, 0, sizeof(delta));
    delta.unique_document_id = strdup("test_doc");
    delta.base_version = 41;
    delta.change_version = 42;
    delta.overlay_id = NULL;
    delta.transaction_count = 2;
    delta.transactions = (Transaction*)malloc(2 * sizeof(Transaction));
    
    delta.transactions[0].type = TRANSACTION_ADDCHARS;
    delta.transactions[0].pos_line = 1;
    delta.transactions[0].pos_col = 5;
    delta.transactions[0].count = 0;
    delta.transactions[0].content = strdup("hello");

    delta.transactions[1].type = TRANSACTION_DELETECHARS;
    delta.transactions[1].pos_line = 1;
    delta.transactions[1].pos_col = 10;
    delta.transactions[1].count = 5;
    delta.transactions[1].content = NULL;

    char *serialized = cb_serialize_delta(&delta);
    printf("Serialized delta: %s\n", serialized);

    Delta *delta2 = cb_deserialize_delta(serialized);
    assert(delta2 != NULL);
    assert(delta2->unique_document_id != NULL);
    assert(strcmp(delta2->unique_document_id, "test_doc") == 0);
    assert(delta2->base_version == 41);
    assert(delta2->change_version == 42);
    assert(delta2->transaction_count == 2);
    assert(delta2->transactions[0].type == TRANSACTION_ADDCHARS);
    assert(strcmp(delta2->transactions[0].content, "hello") == 0);
    assert(delta2->transactions[1].type == TRANSACTION_DELETECHARS);
    assert(delta2->transactions[1].count == 5);

    printf("Delta serialization successful.\n");

    free(delta.unique_document_id);
    free(delta.transactions[0].content);
    free(delta.transactions);
    free(serialized);
    free_delta(delta2);
}

int main() {
    test_tree_flatten_reconstruct();
    test_delta_serialization();
    printf("All serialization tests passed!\n");
    return 0;
}
