# Parser Integration Guide

This guide explains how to build a language parser (server) using the DSL Syntax Highlighter library.

## 1. Implementation
A parser must implement the `ParserFunction` signature.

All `CB_Node.pos`, `CB_Node.length`, transaction columns, and serialized token
ranges are zero-based Unicode codepoint offsets in the logical source buffer.
They are not UTF-8 byte offsets and not rendered screen columns. A parser may
lex byte pointers internally and hand byte spans to DSLSH conversion helpers
such as `cb_utf8_byte_span_to_codepoint_span`,
`cb_line_byte_span_to_codepoint_span`, or
`cb_create_node_from_utf8_byte_span` before publishing nodes.

When one parse projects many byte spans from the same source, build one
`CB_UTF8PositionIndex` and reuse it. The stateless helper rescans from the
start of the source and is intended for occasional conversions, not a token
stream:

```c
CB_UTF8PositionIndex positions;
size_t pos;
size_t length;

if (!cb_utf8_position_index_init(&positions, source, source_byte_length)) {
    /* Handle an unusable byte sequence or allocation failure. */
    return;
}

if (cb_utf8_position_index_span(&positions,
                                token_byte_offset,
                                token_byte_length,
                                &pos,
                                &length)) {
    cb_add_child_node(tb, cb_create_node(LEXER_IDENTIFIER, pos, length));
}

cb_utf8_position_index_free(&positions);
```

The index references rather than copies `source`, so the source must remain
alive and unchanged until `cb_utf8_position_index_free()`. ASCII input uses
direct byte/codepoint identity without an offsets allocation. Non-ASCII input
stores exact codepoint boundaries and resolves spans by binary search. A span
whose start or end falls inside a multibyte character is rejected, as it is by
the stateless converter. Indexed lookups are order-independent.

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
| `LEXER_CONSTANT_IDENTIFIER` | Language/runtime constants, registers. |
| `LEXER_MACRO_IDENTIFIER` | Macro definitions, macro names, macro calls. |
| `LEXER_MACRO_VARIABLE` | Macro/template variables such as `{name}`. |
| `LEXER_MACRO_CONSTANT` | Preprocessor or macro-time constants. |
| `LEXER_PREPROCESSOR` | Compiler directives (`import`, `include`, `.globals`). |
| `LEXER_OPERATOR` | General operators (`:`, `?`). |
| `LEXER_OPERATOR_ASSIGN` | Assignment (`=`). |
| `LEXER_OPERATOR_ARITHMETIC`| Math (`+`, `-`, `*`, `/`). |
| `LEXER_OPERATOR_LOGICAL` | Logic (`&&`, `\|\|`, `!`). |

### Common Issues

#### 1. Column Offset Mismatch
If highlighting is shifted by one or two characters, your parser's column calculation likely differs from the `CodeBuffer`'s view.
- **Rule**: `CodeBuffer` considers the character *after* a `\n` to be column 0 of the next line.
- **Rule**: Columns are Unicode codepoint offsets, not UTF-8 byte offsets.
- **Troubleshooting**: Check your lexer's newline handling. Ensure `linestart` is reset correctly to the cursor position *immediately after* the newline character(s) are consumed. If your lexer uses byte pointers, use the DSLSH byte-span conversion helpers rather than publishing raw byte offsets.

#### 2. Large Files Time Out
If a parser is correct on small sources but slows sharply as token count grows:
- build one `CB_UTF8PositionIndex` per immutable source instead of calling the stateless whole-source conversion for every token;
- index or advance through the parser's own token collection instead of rescanning it from the head for each `cb_add_missing_tokens()` callback; and
- benchmark an optimized build, because parser-mode latency is an end-user path.

#### 3. Protocol Corruption
If the editor reports a crash immediately upon connection:
- **Cause**: Your parser is likely printing debug messages to `stdout`.
- **Fix**: In `stdio` mode, `stdout` is reserved for the hex-encoded SDSLH protocol. Use a dedicated log file or `stderr` for debugging. Use `cb_log_init("my_parser.log")` provided by the library.

#### 4. Hanging / Infinite Loops
If the editor freezes during a parse:
- The library uses a 10-second watchdog in `parser_tester`. If it fails, the parser has a logic error in its tree traversal or lexing loop.
- Ensure your `while` loops always advance the cursor, even on unknown characters.
