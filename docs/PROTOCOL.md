# DSL Syntax Highlighter Protocol (SDSLH)

This document defines the communication protocol between a text editor (client) and a language parser (server).

## 1. Transport Layer
The current implementation uses **TCP Sockets** (default port `8080`). 
Messages are prefixed with a 4-byte network-order length header.

## 2. Message Format
Messages are text-based, using `|` as a field separator. For robustness, string content (source code, error messages) is **hex-encoded**.

### 2.1 Editor -> Parser (Requests)
Requests are prefixed with a type marker:
- **`I|` (Initial Load)**: Sent when a document is first opened.
  - Format: `I|version|line_count|hex_doc_id|hex_full_content|`
- **`D|` (Delta)**: Sent for incremental updates.
  - Format: `D|version|transaction_count|type|line|col|count|hex_content|...`
  - Transaction Types: `a` (Add Chars), `d` (Delete Chars), `L` (Add Line), `l` (Delete Line), `J` (Join Lines), `S` (Split Line).

### 2.2 Parser -> Editor (Response)
The response is always a serialized **Token Stream** representing the updated parse tree.
- Format: `token_count|type|pos|len|identifier_id|severity|hex_msg_code|hex_msg|...`

## 3. Token Types
Tokens are used for both lexical highlighting and structural markers.

### 3.1 Lexer Tokens (Leaf Nodes)
- `33`: `LEXER_WHITESPACE`
- `34`: `LEXER_EOF`
- `35`: `LEXER_TOKEN` (Generic)
- `37`: `LEXER_COMMENT`
- `38`: `LEXER_STRING_LITERAL`
- `39`: `LEXER_NUMBER_LITERAL`
- `40`: `LEXER_KEYWORD`
- `41`: `LEXER_OPERATOR`
- `53`: `LEXER_IDENTIFIER`

### 3.2 Structural Tokens (Control)
- `100`: `TREE_DOWN` - Enter a child scope.
- `101`: `TREE_UP` - Exit to parent scope.

### 3.3 Parse Tree Nodes (Containers)
- `71`: `PARSE_TREE_FILE`
- `73`: `PARSE_TREE_STATEMENT`
- `74`: `PARSE_TREE_EXPR`
- `75`: `PARSE_TREE_COMMENT` (Groups multiple lines)
- `76`: `PARSE_TREE_SCOPE`
- `77`: `PARSE_TREE_FUNCTION`

## 4. Interaction Flow
1. **Initial Load**: Editor sends full file content. Parser returns the initial tree.
2. **Edits**: Editor applies edit to local `CodeBuffer`, performs **Emergency Parsing** (heuristic local shift), and sends a `Delta` to the parser.
3. **Synchronization**: Parser applies `Delta` to its mirror `CodeBuffer`, re-parses, flattens the tree, and returns the new `Token Stream`.
4. **Integration**: Editor replaces its local `CB_ParseTree` with the new result and re-renders.
