/* parser highlighter interface */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "dslsyntax_common.h"

// The actual toy parser function
void toy_parser(CodeBuffer *codeBuffer);

/* Library function to initialize the highlighter with this start source code */
void highlight_init(char* source_code, CodeBuffer **cb, CB_ParseTree **tb);

/* Function to convert AST to CB_ParseTree */
void ast_to_token_buffer(ASTNode *node, CB_ParseTree *tb, CodeBuffer *cb);

/* Function to print AST (provided by user) */
void print_ast(ASTNode *node, int indent);

#endif /* PARSER_H */
