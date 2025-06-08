/* ast.h */

#ifndef AST_H
#define AST_H

//#include "code_buffer.h"  /* For Severity and Message structures */

// Lexer CB_Token Types
typedef enum {
    PARSER_TOKEN_WHITESPACE,
    PARSER_TOKEN_COMMENT,
    PARSER_TOKEN_INT,
    PARSER_TOKEN_IDENTIFIER,
    PARSER_TOKEN_NUMBER,
    PARSER_TOKEN_STRING,
    PARSER_TOKEN_SAY,
    PARSER_TOKEN_ASSIGN,
    PARSER_TOKEN_SEMICOLON,
    PARSER_TOKEN_PLUS,
    PARSER_TOKEN_MINUS,
    PARSER_TOKEN_MULTIPLY,
    PARSER_TOKEN_DIVIDE,
    PARSER_TOKEN_LPAREN,
    PARSER_TOKEN_RPAREN,
    PARSER_TOKEN_EOF,
    PARSER_TOKEN_UNKNOWN,
    PARSER_TOKEN_UNTERMINATED_STRING
} ParserTokenType;

// Lexer CB_Token Structure
typedef struct ParserToken {
    ParserTokenType type;
    size_t token_number; // Token number in a sequence
    char visible; // Whether the token should be visible in the AST
    char *value;
    size_t pos;   // Position in the code_buffer
    size_t length;
    int start_line;
    int start_column;
    int end_line;
    int end_column;
    struct ParserToken *next;
} ParserToken;

/* Enumerations for AST node types */
typedef enum {
    AST_PROGRAM,
    AST_VARIABLE_DECL,
    AST_ASSIGNMENT,
    AST_SAY,
    AST_BINARY_OP,
    AST_NUMBER,
    AST_IDENTIFIER,
    AST_STRING,
    AST_ERROR
} ASTType;

/* Structure for AST Nodes */
typedef struct ASTNode {
    ASTType type;                 /* Type of AST node */
    char *value;                  /* Value (e.g., operator, identifier, etc.) */
    ParserToken *token;           /* Pointer to the token (optional) */
    int line;                     /* Line number in code_buffer */
    int col;                      /* Column number in code_buffer */
    struct ASTNode *parent;       /* Pointer to the node's parent */
    struct ASTNode *child;        /* Pointer to the first child node */
    struct ASTNode *sibling;      /* Pointer to the next sibling node */
} ASTNode;

#endif /* AST_H */
