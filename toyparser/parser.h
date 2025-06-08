// Parser Header File
#ifndef TOYPARSER_PARSER_H_
#define TOYPARSER_PARSER_H_
#include "ast.h"

// Lexer state
typedef struct {
    const char *text;
    size_t pos;
    char current_char;
    int line;
    int column;
} Lexer;

// Function prototypes
void lexer_advance(Lexer *lexer);
void lexer_skip_whitespace(Lexer *lexer);
void lexer_skip_comment(Lexer *lexer);
ParserToken *lexer_get_next_token(Lexer *lexer);
ParserToken *lexer_number(Lexer *lexer);
ParserToken *lexer_identifier(Lexer *lexer);
ParserToken *lexer_string(Lexer *lexer);

ASTNode *parse_program(Lexer *lexer);
ASTNode *parse_statement(Lexer *lexer);
ASTNode *parse_expression(Lexer *lexer);
ASTNode *parse_mulexp(Lexer *lexer);
ASTNode *parse_term(Lexer *lexer);
ASTNode *parse_error(const char *message);
void panic_mode(Lexer *lexer);

void print_ast(ASTNode *node, int indent);
void free_ast(ASTNode *node);
void free_token_list();
ParserToken *get_token_by_pos(size_t pos);

extern ParserToken *head;


#endif  // TOYPARSER_PARSER_H_