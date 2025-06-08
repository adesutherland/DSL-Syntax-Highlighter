### **1. Token Types**

The protocol uses two sets of tokens to handle syntax and structure:

- **Lexer Tokens**: Provide the foundation for syntax highlighting by representing lexical elements from the source code.
- **Parse Tree Tokens**: Provide structural information, allowing the editor to handle code folding and understand the logical hierarchy of the code.

#### 1.1. **Lexer Tokens**

Lexer Tokens represent **lexical elements** from the source code and are critical for syntax highlighting. These tokens are **sequential** with no gaps, ensuring the entire source code is covered. Each token has a **length** attribute to maintain line and column information. The lexer tokens are kept generic to be adaptable to various languages.

##### **1.1.1. Token Types**

- **Whitespace Tokens**:
    - `WHITESPACE`: Represents spaces or tabs.
    - `NEWLINE`: Represents line breaks.

- **Comment Tokens**:
    - `LINE_COMMENT`: Represents a comment that starts with a line comment marker (e.g., `//`).
    - `COMMENT_LINE`: Represents a single line in a block comment (multi-line comments are split into lines).

- **Literal Tokens**:
    - `STRING_LITERAL`: Represents string literals.
    - `NUMBER_LITERAL`: Represents integer or floating-point numbers.

- **Keyword Tokens**:
    - `KEYWORD`: A generic token representing language-specific keywords. This can optionally be extended with more specific keywords like `KEYWORD_CONTROL` (e.g., `if`, `else`, `while`) and `KEYWORD_TYPE` (e.g., `int`, `float`), depending on the language needs.

- **Operator Tokens**:
    - `OPERATOR`: A generic operator token for any operator.
    - **Specific operators**:
        - `OPERATOR_ASSIGN`: Assignment operators (`=`).
        - `OPERATOR_ARITHMETIC`: Arithmetic operators (`+`, `-`, `*`, `/`).
        - `OPERATOR_LOGICAL`: Logical operators (`&&`, `||`).

- **Separator Tokens**:
    - `SEPARATOR`: A generic separator token for punctuation or delimiters (e.g., `,`, `;`, `()`, `[]`, `{}`).
    - You can optionally extend this with specific separators like `SEPARATOR_COMMA`, `SEPARATOR_SEMICOLON`, etc., but it's not required since **Parse Tree Tokens** will cover grouping and code blocks.

- **Identifier Tokens**:
    - `IDENTIFIER`: Represents user-defined names such as variable names, function names, etc. This token can include an **optional `unique_id` attribute** to help track identifiers within the correct scope.

##### **1.1.2. Token Attributes**

- **Length**: Every lexer token includes a length to indicate the number of characters it covers. This is especially important for maintaining line and column information in the editor.

- **Identifier Unique ID**: The `IDENTIFIER` token has an optional `unique_id` to uniquely identify the symbol across scopes, facilitating operations like renaming or searching.

##### **1.1.3. Example Usage**

For a simple line of code like:
```c
int x = 5 + y;
```

The tokens might be represented as:
```
KEYWORD_TYPE IDENTIFIER OPERATOR_ASSIGN NUMBER_LITERAL OPERATOR_ARITHMETIC IDENTIFIER SEPARATOR_SEMICOLON
```

---

#### 1.2. **Parse Tree Tokens**

Parse Tree Tokens help represent the **structure** of the code, which is especially useful for features like **code folding** or **scope-aware refactoring**. These tokens don’t have lengths and are simply used to organize and represent the hierarchical structure of the code.

##### **1.2.1. Parse Tree Token Types**

- **Tree Control Tokens**:
    - `TREE_START`: Marks the beginning of a code block or structural element.
    - `TREE_END`: Marks the end of the current_parent block or structure.

- **Specific Structure Tokens**:
    - `TREE_CODEBLOCK`: Represents a code block (e.g., function body, class definition, loops).
    - `TREE_STATEMENT`: Represents an individual statement (e.g., a control structure or command).
    - `TREE_EXPR`: Represents an expression (e.g., binary operations).
    - `TREE_COMMENT`: Groups multiple line comments into a comment block.
    - `TREE_SCOPE`: Represents namespaces or modular scope (e.g., classes, namespaces).
    - `TREE_FUNCTION`: Represents functions or methods.
    - `TREE_STRUCTURE`: Represents higher-level structures like classes, structs, or modules.

##### **1.2.2. Example Parse Tree Tokens**

For a code snippet:
```c
int x = 5 + y;
if (x > 10) {
    do_something();
}
```

The tokens might be:
```
TREE_STATEMENT KEYWORD_TYPE IDENTIFIER OPERATOR_ASSIGN NUMBER_LITERAL OPERATOR_ARITHMETIC IDENTIFIER SEPARATOR_SEMICOLON TREE_END
TREE_CODEBLOCK KEYWORD_CONTROL SEPARATOR_PARENTHESIS_OPEN IDENTIFIER OPERATOR_LOGICAL NUMBER_LITERAL SEPARATOR_PARENTHESIS_CLOSE TREE_CODEBLOCK KEYWORD_FUNCTION CALL SEPARATOR_SEMICOLON TREE_END TREE_END
```

The `TREE_STATEMENT` token encapsulates a single statement (`int x = 5 + y;`), and the `TREE_CODEBLOCK` token encapsulates the `if` block and its contents. This information allows editors to fold or expand the structure, helping with readability and navigation.

---

### 2. **Token Stream Structure and Synchronization**

The token stream consists of both **Lexer Tokens** and **Parse Tree Tokens**, ordered sequentially to ensure the editor can process them correctly. **Lexer Tokens** handle highlighting and tokenization, while **Parse Tree Tokens** handle code structure and folding.

- **Lexer Tokens** ensure that there are no gaps in the token stream, meaning all characters are accounted for.
- **Parse Tree Tokens** are used for organizing and marking structure, allowing for tree-like representations of the code.

---

### 3. **Protocol Details**

##### 3.1. **Token Codes**

- **ASCII Encoding**: All tokens are encoded using **printable ASCII characters** to allow easy transmission over networks.
    - **Lexer Tokens**: Begin at ASCII 33 (`!`) and cover common lexical elements (e.g., keywords, operators, separators).
    - **Parse Tree Tokens**: Assigned within the same printable ASCII range to keep the protocol simple and readable.

##### 3.2. **INFO Tokens**

The protocol includes a mechanism to provide information or handle errors through **INFO tokens**.

- **INFO**: Provides general information or warnings associated with a token. The token has a `message` and `severity` attribute, allowing the editor to display this information alongside the token.
- **INFO_START / INFO_END**: These tokens are used to mark a range of tokens that should be highlighted as part of an informational block (e.g., a warning that spans multiple tokens).
    - `INFO_START`: Begins a range of tokens where information applies.
    - `INFO_END`: Ends the informational block.
- **Severity Levels**: `INFO` tokens can have varying severity, including `INFO_WARNING`, `INFO_ERROR`, or `INFO_NOTE`, depending on the nature of the information.

##### 3.3. **Token Flow and Synchronization**

1. **Initialization**: The editor initializes a **code buffer** with the entire source code and sends it to the parser.

2. **Tokenization**: The parser generates the appropriate **Lexer Tokens** and **Parse Tree Tokens** and sends them to the editor.

3. **Updates and Transactions**:
    - The editor processes **key presses** and applies changes to both the **code buffer** and the **token buffer**.
    - For real-time feedback (e.g., emergency syntax highlighting), minimal changes (like extending tokens) are applied until the parser sends updated tokens.

4. **Synchronizing Changes**: The parser can send back only the **modified tokens** between a first and last changed token, optimizing the communication between the editor and the parser.

---

### 4. **Example Use Case**

Consider the following source code with both valid and invalid syntax:

```c
int a = 10; 
/* Invalid statement */ 
a = ;
```

The **Lexer Tokens** would handle tokenization for each element:
```
KEYWORD_TYPE IDENTIFIER OPERATOR_ASSIGN NUMBER_LITERAL SEPARATOR_SEMICOLON NEWLINE COMMENT_LINE IDENTIFIER OPERATOR_ASSIGN INFO_ERROR SEPARATOR_SEMICOLON
```

The **Parse Tree Tokens** would represent the structure:
```
TREE_STATEMENT KEYWORD_TYPE IDENTIFIER OPERATOR_ASSIGN NUMBER_LITERAL SEPARATOR_SEMICOLON TREE_END
TREE_COMMENT COMMENT_LINE TREE_END
TREE_STATEMENT IDENTIFIER OPERATOR_ASSIGN INFO_ERROR SEPARATOR_SEMICOLON TREE_END
```

In this case, the `INFO_ERROR` token indicates a syntax error (`a = ;`), and the `TREE_COMMENT` encapsulates a multi-line comment.
