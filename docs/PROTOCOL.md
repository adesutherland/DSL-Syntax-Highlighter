# DSL Syntax Highlighter Protocol (DSLSH)

This document defines the communication protocol between a text editor (client) and a language parser (server).

## 1. Transport Layer
DSLSH supports two primary transport methods for out-of-process communication:

### 1.1 STDIN/STDOUT (Default)
This is the recommended transport for local language parsers. The editor launches the parser as a child process and communicates via pipes.
- **Framing**: Every message is prefixed with an **8-character hex string** representing the payload length (e.g., `000000ff` for 255 bytes).
- **Child Management**: The editor is responsible for spawning and terminating the child process.

### 1.2 TCP Sockets
Alternative transport for remote parsers or persistent servers.
- **Framing**: Identical to STDIO (**8-character hex length header**).
- **Default Port**: `8080`.

## 2. Message Format
Messages are text-based, using `|` as a field separator. For robustness, string content (source code, error messages) is **hex-encoded**.

### 2.1 Editor -> Parser (Requests)
Requests are prefixed with a type marker:
- **`I|` (Initial Load)**: Sent when a document is first opened.
  - Format: `I|version|line_count|hex_doc_id|hex_full_content|`
- **`D|` (Delta)**: Sent for incremental updates.
  - Version 2 format: `D|2|hex_doc_id|base_version|new_version|hex_overlay_id|transaction_count|type|line|col|count|hex_content|...`
  - Legacy format: `D|version|transaction_count|type|line|col|count|hex_content|...`
- **`H|` (Hypothesis Delta)**: Sent for non-committing parser feedback, such as code-completion previews.
  - Format: `H|2|hex_doc_id|base_version|hypothesis_version|hex_overlay_id|transaction_count|type|line|col|count|hex_content|...`
  - The parser applies the transactions to a scratch copy and returns a token stream without changing the committed document mirror.
  - Editors should discard hypothesis responses whose `base_version` no longer matches the real buffer.
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
3. **Synchronization**: Parser applies `Delta` to the matching document mirror `CodeBuffer`, re-parses, flattens the tree, and returns the new `Token Stream`.
4. **Integration**: Editor replaces its local `CB_ParseTree` with the new result and re-renders.

For parsers that serve more than one document in a single process, `hex_doc_id`
selects the document session. A parser must create or replace that session on
`I|`, mutate only that session on `D|`, and leave all committed sessions
unchanged on `H|`.
