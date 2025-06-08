

#include "token_buffer.h"

int main() {
    CB_ParseTree *tb;
    CB_Node token;

    /* Create a new CB_ParseTree */
    tb = cb_create_token_buffer();

    /* Add Root Node */
    token = cb_create_node(PARSE_TREE, 0, 0);
    cb_add_child_node(tb, token);
    cb_set_current_parent_to_root_node(tb);

    /* Add a keyword token */
    token = cb_create_node(LEXER_KEYWORD, 0, 3);
    cb_add_child_node(tb, token);

    /* Add a string token */
    token = cb_create_node(LEXER_STRING_LITERAL, 3, 12);
    cb_add_child_node(tb, token);

    /* Add an ERROR token with a message */
    token = cb_create_node(SYNTAX_ERROR, 3, 12);
    token.severity = CB_ERROR;
    token.message = "Invalid string";

    cb_set_current_parent_to_last_node(tb);
    cb_add_child_node(tb, token);

    /* Print the CB_ParseTree */
    cb_print_token_buffer(NULL, tb);

    /* Free the CB_ParseTree */
    cb_free_token_buffer(tb);

    return 0;
}

