#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dslsyntax_common.h"
#include "dslsyntax_parser.h"
#include "dslsyntax_log.h"

/* 
 * A fake parser function that always returns a tree with 
 * one big comment and one keyword.
 */
void fake_parser_func(CodeBuffer *cb) {
    LOG("fake_parser_func: building static tree");
    
    CB_ParseTree *tb = cb_create_token_buffer();
    
    CB_Node root = cb_create_node(PARSE_TREE_FILE, 0, get_code_buffer_length(cb));
    cb_add_child_node(tb, root);
    cb_set_current_parent_to_root_node(tb);

    /* Add a comment at the start */
    cb_add_child_node(tb, cb_create_node(LEXER_COMMENT, 0, 10));
    
    /* Add a keyword at pos 11 */
    cb_add_child_node(tb, cb_create_node(LEXER_KEYWORD, 11, 5));
    
    cb->parse_tree = tb;
}

int main(int argc, char *argv[]) {
    int port = 8081;
    if (argc > 1) port = atoi(argv[1]);

    cb_log_init(NULL);
    LOG("Fake Parser Server starting on port %d...", port);

    CodeBuffer *cb = create_code_buffer(NULL, fake_parser_func);
    cb_start_server(cb, "127.0.0.1", port);

    free_code_buffer(cb);
    return 0;
}
