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

## 4. Testing and Troubleshooting

Testing a background parser interactively in an editor is difficult. Use the automated tools provided.

### Automated Testing with `parser_tester`
The `parser_tester` tool allow you to simulate editor transactions and verify the AST without a GUI.

1. **Create a test script** (`test.txt`):
   ```text
   # Initialize the parser
   INIT "./bin/my_parser --syntaxhighlight" test.src
   
   # Check initial AST
   DUMP_AST
   
   # Test Emergency Parsing (Zero-latency feedback)
   # INSERT_ASYNC sends the edit but DUMP_AST will show the heuristic tree 
   # before the background parser finishes.
   INSERT_ASYNC 0 0 // New content\n
   DUMP_AST
   
   # Wait for background parser to finish and check final tree
   SYNC
   DUMP_AST
   
   QUIT
   ```
2. **Run the test**:
   ```bash
   parser_tester -q test.txt
   ```
3. **Use Golden Files**: Use the `-g` flag to compare the output against a known good baseline. This is essential for regression testing.
   ```bash
   parser_tester -q -g my_test.golden test.txt
   ```

### Best Practices for Token Mapping

To ensure your parser looks good in all SDSLH-compatible editors (like THE and `te`), map your language-specific tokens to the middleware types consistently:

| Middleware Type | Recommended Usage |
| :--- | :--- |
| `LEXER_KEYWORD` | Reserved words, built-in commands, opcodes. |
| `LEXER_IDENTIFIER` | Variable names, label definitions, general symbols. |
| `LEXER_FUNCTION_IDENTIFIER` | Function names, method calls, labels. |
| `LEXER_CONSTANT_IDENTIFIER` | Constants, macros, registers. |
| `LEXER_PREPROCESSOR` | Compiler directives (`import`, `include`, `.globals`). |
| `LEXER_OPERATOR` | General operators (`:`, `?`). |
| `LEXER_OPERATOR_ASSIGN` | Assignment (`=`). |
| `LEXER_OPERATOR_ARITHMETIC`| Math (`+`, `-`, `*`, `/`). |
| `LEXER_OPERATOR_LOGICAL` | Logic (`&&`, `\|\|`, `!`). |

### Common Issues

#### 1. Column Offset Mismatch
If highlighting is shifted by one or two characters, your parser's column calculation likely differs from the `CodeBuffer`'s view.
- **Rule**: `CodeBuffer` considers the character *after* a `\n` to be column 0 of the next line.
- **Troubleshooting**: Check your lexer's newline handling. Ensure `linestart` is reset correctly to the cursor position *immediately after* the newline character(s) are consumed.

#### 2. Protocol Corruption
If the editor reports a crash immediately upon connection:
- **Cause**: Your parser is likely printing debug messages to `stdout`.
- **Fix**: In `stdio` mode, `stdout` is reserved for the hex-encoded SDSLH protocol. Use a dedicated log file or `stderr` for debugging. Use `cb_log_init("my_parser.log")` provided by the library.

#### 3. Hanging / Infinite Loops
If the editor freezes during a parse:
- The library uses a 10-second watchdog in `parser_tester`. If it fails, the parser has a logic error in its tree traversal or lexing loop.
- Ensure your `while` loops always advance the cursor, even on unknown characters.
