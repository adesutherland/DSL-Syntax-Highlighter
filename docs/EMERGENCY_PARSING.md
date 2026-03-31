# Emergency Parsing Rules

Emergency Parsing is a core platform feature of SDSLH that provides immediate visual feedback to users while a full parse is being performed asynchronously by the server. 

Because it must be extremely lightweight and language-agnostic, it relies on a set of robust heuristics rather than a full grammar.

## 1. Inheritance Rules
When new characters are typed (`TRANSACTION_ADDCHARS`), they inherit attributes from their neighbors:
- **Left-Neighbor Preference**: New characters primarily inherit the `token_type` and `severity` of the character to their immediate left. This ensures that continuing a keyword or a string feels natural.
- **Start-of-Line Fallback**: If typing at the very beginning of a line, attributes are inherited from the character that was previously at that position.

## 2. Character-Level Overrides
Some characters always trigger immediate type changes regardless of their neighbors:
- **Whitespace**: Any character identified as a space or tab is automatically forced to `LEXER_WHITESPACE`.

## 3. Line-Based Heuristics
After an edit is applied, the library performs a single-pass scan of the modified line to "patch" common lexical structures:

### 3.1 Comments
- **Line Comments**: Detecting `//` forces all subsequent characters on that line to `LEXER_COMMENT`.
- **Block Comment Start**: Detecting `/*` forces all subsequent characters on that line to `LEXER_COMMENT`. (Note: Since emergency parsing is line-based, multi-line block comment closure is handled by the real parser).

### 3.2 Strings
- **Double/Single Quotes**: Detecting `"` or `'` toggles a "string mode" for the remainder of the line. All characters within the quotes are forced to `LEXER_STRING_LITERAL`.

### 3.3 Numeric Literals
- **Digits**: Characters `0-9` are identified as `LEXER_NUMBER_LITERAL`, provided they are not inside a string or comment.

## 4. Tree Shifting
The emergency parser also maintains the integrity of the hierarchical `CB_ParseTree`:
- **Positional Shifting**: Every node in the tree that starts after the edit point has its `pos` shifted by the length of the edit.
- **Length Extension**: Any node that "contains" the edit point (e.g., a function body or a statement) has its `length` automatically extended or shrunk.

## 5. Reconciliation
Emergency parsing is **temporary**. Once the parser server returns a perfectly accurate `Token Stream`, the library:
1. Clears the heuristic attributes.
2. Replaces the shifted tree with the new, authoritative tree.
3. Re-highlights the buffer for 100% accuracy.
