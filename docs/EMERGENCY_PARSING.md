# Emergency Parsing Rules

Emergency Parsing (EP) is a core platform feature of SDSLH that provides immediate, heuristic visual feedback to users while a full parse is being performed asynchronously by the server. 

Because it must be fast and execute before the parser finishes, the emergency parsing engine has been completely redesigned into a **Data-Driven, "Do No Harm" System**.

## 1. The "Do No Harm" Principle (Immutable Parsed Nodes)
The most critical rule of Emergency Parsing is that it **never overrides an authoritatively parsed token**. 
If a character has been assigned a valid `node` pointer by the background parser, the Emergency Parser treats it as an immutable "wall." It will read the character to maintain state (e.g., to know if it's inside a string or comment), but it will never change its `token_type` or color.

The EP scanner's sole job is to "fill in the blanks" (characters where `node == NULL`).

## 2. Smart "Copy-Left" Extension
When a user types new characters (`TRANSACTION_ADDCHARS`), they must inherit types immediately so that typing feels continuous.
- **Continuations:** If the user types an alphanumeric character immediately following an existing `LEXER_IDENTIFIER` or `LEXER_KEYWORD`, or types inside an existing `LEXER_STRING_LITERAL` or `LEXER_COMMENT`, the new character completely adopts the left neighbor's `token_type` **and its `node` pointer**. This means it is treated as part of the parsed token and protected from the heuristic scanner.
- **Breaks:** If the user types a space, or transitions between alphanumeric and punctuation, the "chain" is broken. The new character gets `node = NULL` and `token_type = LEXER_TOKEN` (or `LEXER_WHITESPACE`), marking it as unparsed territory for the EP scanner to evaluate.

## 3. Data-Driven Heuristics (Learning & Seeding)
The EP scanner no longer relies on hardcoded C-style rules (like `//` or `"`). Instead, it dynamically uses an `EP_Rules` configuration.

### 3.1 Seeding (Pre-Configuration)
Before the first parse completes, the system can be "seeded" with rules.
- **`ep_rules.conf`**: The system loads a configuration file containing definitions for file extensions or shebang patterns.
- **Detection**: When a file is opened, `cb_seed_ep_rules` uses the file extension or the first line's shebang (e.g., `#!/usr/bin/python`) to match against `ep_rules.conf`. If a match is found, keywords, operators, strings, and comment patterns are loaded immediately.

### 3.2 Dynamic Learning
Once the background parser returns a `CB_ParseTree`, the system actively **learns** the language.
- `cb_learn_ep_rules` traverses the new AST. 
- It extracts the literal text of nodes marked as `LEXER_KEYWORD`, `LEXER_OPERATOR`, `LEXER_STRING_LITERAL`, and `LEXER_COMMENT`.
- It ignores any tokens marked with `CB_ERROR` to prevent learning from temporary syntax mistakes.
- It dynamically builds or extends the `EP_Rules` for that specific document.

## 4. Line-Based Scanner
When transactions occur, the modified line is scanned using the active `EP_Rules`:
- **Strings**: It tracks string openings and closings based on learned quote characters.
- **Comments**: It detects line-comment prefixes (e.g., `//`, `#`) and block-comment prefixes (`/*`, `<!--`). Because EP is line-based for performance, a block comment start will simply highlight the remainder of that specific line until the full parser returns.
- **Keywords**: It searches for learned keywords within unparsed gaps. It strictly enforces **word boundaries** (spaces, symbols, or parsed nodes) to prevent partial matches.
- **Operators**: It matches operators, prioritizing longer operators (e.g., `+=`) before shorter ones (`+`).

## 5. Reconciliation
Emergency parsing is **temporary**. Once the parser server returns a perfectly accurate `Token Stream`:
1. The old AST is discarded.
2. The new, authoritative AST is applied to the buffer.
3. The learner extracts any new rules.
4. The buffer is completely re-highlighted with 100% accuracy, and any heuristic guesses made in the interim are replaced by authoritative tokens.