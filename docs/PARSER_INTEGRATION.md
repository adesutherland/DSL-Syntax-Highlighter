# Parser Integration Guide

This guide explains how to build a language parser (server) using the DSL Syntax Highlighter library.

## 1. Implementation
A parser must implement the `ParserFunction` signature.

```c
void my_language_parser(CodeBuffer *cb) {
    // 1. Get the source code
    char *source = get_code_buffer_source(cb);

    // 2. Create a new parse tree
    CB_ParseTree *tb = cb_create_token_buffer();

    // 3. Build the tree (Hierarchical)
    CB_Node root = cb_create_node(PARSE_TREE_FILE, 0, total_len);
    cb_add_child_node(tb, root);
    cb_set_current_parent_to_root_node(tb);

    // Add containers and leaf tokens
    CB_Node stmt = cb_create_node(PARSE_TREE_STATEMENT, 0, 10);
    cb_add_child_node(tb, stmt);
    cb_set_current_parent_to_last_node(tb);
    
    cb_add_child_node(tb, cb_create_node(LEXER_KEYWORD, 0, 3));
    
    // 4. Set the result in the buffer
    cb->parse_tree = tb;
}
```

## 2. Advanced Tree Smarts
The library provides utilities to simplify tree construction:
- `cb_order_tree(tb)`: Sorts children by position and updates parent lengths.
- `cb_add_missing_tokens(tb, cb, ...)`: Fills gaps in the tree with whitespace or comments.
- `cb_tweak_tree_positions(tb)`: Heuristically moves terminators and brackets into their relevant subtrees.

## 3. Running the Server
Use the platform server loop to handle socket communication.

```c
#include "dslsyntax_parser.h"

int main() {
    // Create CodeBuffer with your parser
    CodeBuffer *cb = create_code_buffer(NULL, my_language_parser);

    // Start the socket server (blocks)
    cb_start_server(cb, "127.0.0.1", 8080);
}
```
