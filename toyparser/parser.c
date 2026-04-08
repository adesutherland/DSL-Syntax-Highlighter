#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "dslsyntax_log.h"
//#include "ast.h"

static char *parser_strndup(const char *source, size_t length) {
    char *copy = malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

/* Global variables for simplicity */
// ParserToken linked list head and tail
ParserToken *head = NULL;
static ParserToken *tail = NULL;
ParserToken* current_token_global;


// Lexer functions

// Function to create a new ParserToken and add it to this linked list
ParserToken *add_token(ParserTokenType type, char visible, char *value, size_t pos, size_t length, int start_line, int start_column, int end_line, int end_column) {
    ParserToken *token = malloc(sizeof(ParserToken));
    token->type = type;
    token->visible = visible;
    token->value = value ? strdup(value) : NULL;
    token->pos = pos;
    token->length = length;
    token->start_line = start_line;
    token->start_column = start_column;
    token->end_line = end_line;
    token->end_column = end_column;
    token->next = NULL;

    if (head == NULL) {
        token->token_number = 0;
        head = token;
        tail = token;
    } else {
        token->token_number = tail->token_number + 1;
        tail->next = token;
        tail = token;
    }

    return token;
}

// Function to free the linked list
void free_token_list() {
    ParserToken *current = head;
    ParserToken *next;
    while (current != NULL) {
        next = current->next;
        if (current->value) free(current->value);
        free(current);
        current = next;
    }
    head = NULL;
    tail = NULL;
}

ParserToken *get_token_by_pos(size_t pos) {
    ParserToken *current = head;
    while (current != NULL) {
        if (current->pos == pos) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void lexer_advance(Lexer *lexer) {
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 0;
    } else {
        lexer->column++;
    }
    lexer->pos++;
    if (lexer->pos >= strlen(lexer->text)) {
        lexer->current_char = '\0';
    } else {
        lexer->current_char = lexer->text[lexer->pos];
    }
}

void lexer_skip_whitespace(Lexer *lexer) {
    size_t pos = lexer->pos;
    int line = lexer->line;
    int column = lexer->column;

    while (lexer->current_char != '\0' && isspace(lexer->current_char)) {
        lexer_advance(lexer);
    }

    if (pos != lexer->pos) {
        add_token(PARSER_TOKEN_WHITESPACE, 0, 0, pos, lexer->pos - pos, line, column, lexer->line, lexer->column);
    }
}

void lexer_skip_comment(Lexer *lexer) {
    size_t pos = lexer->pos;
    int line = lexer->line;
    int column = lexer->column;

    while (lexer->current_char != '\0' && lexer->current_char != '\n') {
        lexer_advance(lexer);
    }

    if (pos != lexer->pos) {
        add_token(PARSER_TOKEN_COMMENT, 0, 0, pos, lexer->pos - pos, line, column, lexer->line, lexer->column);
    }
}

ParserToken* lexer_number(Lexer *lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->pos;

    while (lexer->current_char != '\0' && isdigit(lexer->current_char)) {
        lexer_advance(lexer);
    }
    size_t length = lexer->pos - start_pos;
    char *number_str = parser_strndup(lexer->text + start_pos, length);

    ParserToken* token = add_token(PARSER_TOKEN_NUMBER, 1, number_str, start_pos, length, start_line, start_column, lexer->line, lexer->column);
    free(number_str); // Free the number string after adding the token
    return token;
}

ParserToken *lexer_identifier(Lexer *lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->pos;

    while (lexer->current_char != '\0' && (isalnum(lexer->current_char) || lexer->current_char == '_')) {
        lexer_advance(lexer);
    }
    size_t length = lexer->pos - start_pos;
    char *id_str = parser_strndup(lexer->text + start_pos, length);

    ParserTokenType type = PARSER_TOKEN_IDENTIFIER;
    if (strcmp(id_str, "int") == 0) {
        type = PARSER_TOKEN_INT;
    } else if (strcmp(id_str, "say") == 0) {
        type = PARSER_TOKEN_SAY;
    }

    ParserToken* token = add_token(type, 1, id_str, start_pos, length, start_line, start_column, lexer->line, lexer->column);
    free(id_str); // Free the identifier string after adding the token
    return token;
}

ParserToken *lexer_string(Lexer *lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->pos;
    char quote = lexer->current_char;
    lexer_advance(lexer); // Skip the opening quote

    while (lexer->current_char != '\0' && lexer->current_char != '\n' && lexer->current_char != quote) {
        lexer_advance(lexer);
    }
    if (lexer->current_char == quote) {
        size_t length = lexer->pos - start_pos + 1;
        char *string_str = parser_strndup(lexer->text + start_pos, length);
        lexer_advance(lexer); // Skip the closing quote

        ParserToken *token;
        token = add_token(PARSER_TOKEN_STRING, 1, string_str, start_pos, length, start_line, start_column, lexer->line, lexer->column);
        free(string_str); // Free the string after adding the token
        return token;
    } else {
        // Unterminated string
        ParserToken* token = add_token(PARSER_TOKEN_UNTERMINATED_STRING, 1, 0, start_pos, lexer->pos - start_pos, start_line, start_column, lexer->line, lexer->column);
        return token;
    }
}

ParserToken* lexer_get_next_token(Lexer *lexer) {
    while (lexer->current_char != '\0') {

        if (isspace(lexer->current_char)) {
            lexer_skip_whitespace(lexer);
            continue;
        }

        if (lexer->current_char == '#') {
            lexer_skip_comment(lexer);
            continue;
        }

        if (lexer->current_char == '/') {
            if (lexer->text[lexer->pos + 1] == '/') {
                lexer_skip_comment(lexer);
                continue;
            } else if (lexer->text[lexer->pos + 1] == '*') {
                /* Block comment */
                size_t start_pos = lexer->pos;
                int start_line = lexer->line;
                int start_col = lexer->column;
                lexer_advance(lexer); // /
                lexer_advance(lexer); // *
                while (lexer->current_char != '\0' && !(lexer->current_char == '*' && lexer->text[lexer->pos + 1] == '/')) {
                    lexer_advance(lexer);
                }
                if (lexer->current_char == '*') {
                    lexer_advance(lexer); // *
                    lexer_advance(lexer); // /
                }
                add_token(PARSER_TOKEN_COMMENT, 0, 0, start_pos, lexer->pos - start_pos, start_line, start_col, lexer->line, lexer->column);
                continue;
            }
        }

        if (isdigit(lexer->current_char)) {
            return lexer_number(lexer);
        }

        if (isalpha(lexer->current_char) || lexer->current_char == '_') {
            return lexer_identifier(lexer);
        }

        if (lexer->current_char == '"' || lexer->current_char == '\'') {
            return lexer_string(lexer);
        }

        if (lexer->current_char == '=') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_ASSIGN, 1, "=", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        if (lexer->current_char == ';') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_SEMICOLON, 1, ";", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        if (lexer->current_char == '+') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_PLUS, 1, "+", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        if (lexer->current_char == '-') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_MINUS, 1, "+", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        if (lexer->current_char == '*') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_MULTIPLY, 1, "+", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        if (lexer->current_char == '/') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_DIVIDE, 1, "+", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        if (lexer->current_char == '(') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_LPAREN, 1, "(", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        if (lexer->current_char == ')') {
            int start_line = lexer->line;
            int start_column = lexer->column;
            size_t start_pos = lexer->pos;
            lexer_advance(lexer);
            ParserToken *token = add_token(PARSER_TOKEN_RPAREN, 1, ")", start_pos, 1, start_line, start_column, lexer->line, lexer->column);
            return token;
        }

        // Unknown character
        int start_line = lexer->line;
        int start_column = lexer->column;
        size_t start_pos = lexer->pos;
        char unknown_char = lexer->current_char;
        lexer_advance(lexer);
        char *unknown_str = malloc(2);
        unknown_str[0] = unknown_char;
        unknown_str[1] = '\0';
        ParserToken *token = add_token(PARSER_TOKEN_UNKNOWN, 0, unknown_str, start_pos, 1, start_line, start_column, lexer->line, lexer->column);
        return token;
    }

    // End of input
    ParserToken *token = add_token(PARSER_TOKEN_EOF, 0, 0, lexer->pos, 0, lexer->line, lexer->column, lexer->line, lexer->column);
    return token;
}

// Parsing functions
// Returning 1 means the parsing was successful
int eat(Lexer *lexer, ParserTokenType type) {
    if (current_token_global->type == type) {
        current_token_global = lexer_get_next_token(lexer);
        return 1;
    } else {
        return 0;
    }
}

// Check if the next token is semicolon
// If it isn't, create an error node and add it to the tree
// and call panic_mode, and return 0
int check_semicolon(Lexer *lexer, ASTNode* statement) {
    if (current_token_global->type == PARSER_TOKEN_SEMICOLON) {
        current_token_global = lexer_get_next_token(lexer);
        return 1;
    } else {
        // Create an error node for missing semicolon
        ASTNode *error_node = parse_error("Expected semicolon after statement");
        // Add it to the tree, the last child of the current_parent node
        error_node->parent = statement;
        ASTNode *n = statement->child;
        if (!n) {
            statement->child = error_node;
        } else {
            while (n->sibling != NULL) {
                n = n->sibling;
            }
            n->sibling = error_node;
        }
        panic_mode(lexer);
        return 0;
    }
}

ASTNode *create_ast_node(ASTType type, const char *value, ParserToken* token) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = type;
    node->token = token;
    node->value = value ? strdup(value) : NULL;
    node->child = NULL;
    node->sibling = NULL;
    node->parent = NULL;
    return node;
}

ASTNode *parse_program(Lexer *lexer) {
    current_token_global = lexer_get_next_token(lexer);
    ASTNode *root = create_ast_node(AST_PROGRAM, NULL, NULL);

    ASTNode *last_stmt = NULL;

    while (current_token_global->type != PARSER_TOKEN_EOF) {
        ASTNode *stmt = parse_statement(lexer);

        if (root->child == NULL) {
            root->child = stmt;
        } else {
            if (last_stmt) last_stmt->sibling = stmt;
        }
        last_stmt = stmt;
    }

    return root;
}

ASTNode *parse_statement(Lexer *lexer) {
    ASTNode *node = NULL;
    if (current_token_global->type == PARSER_TOKEN_INT) {
        ParserToken *variable_decl_token = current_token_global;
        // Variable declaration
        eat(lexer, PARSER_TOKEN_INT);
        if (current_token_global->type != PARSER_TOKEN_IDENTIFIER) {
            node = parse_error("Expected identifier after 'int'");
            panic_mode(lexer);
            return node;
        }
        char *var_name = current_token_global->value;
        ParserToken *var_name_token = current_token_global;
        eat(lexer, PARSER_TOKEN_IDENTIFIER);

        if (current_token_global->type == PARSER_TOKEN_ASSIGN) {
            eat(lexer, PARSER_TOKEN_ASSIGN);
            ASTNode *expr_node = parse_expression(lexer);
            node = create_ast_node(AST_VARIABLE_DECL, NULL, variable_decl_token);
            ASTNode *identifier_node = create_ast_node(AST_IDENTIFIER, var_name, var_name_token);
            identifier_node->parent = node;
            expr_node->parent = node;
            node->child = identifier_node;
            identifier_node->sibling = expr_node;
            if (expr_node->type == AST_ERROR) {
                return node;
            }
        } else {
            node = create_ast_node(AST_VARIABLE_DECL, NULL, variable_decl_token);
            ASTNode *identifier_node = create_ast_node(AST_IDENTIFIER, var_name, var_name_token);
            identifier_node->parent = node;
            node->child = identifier_node;
        }
        check_semicolon(lexer, node);

    } else if (current_token_global->type == PARSER_TOKEN_IDENTIFIER) {
        // Assignment
        // char *var_name = strdup(current_token_global->value);
        char *var_name = current_token_global->value;
        ParserToken *var_name_token = current_token_global;
        eat(lexer, PARSER_TOKEN_IDENTIFIER);

        ParserToken *assignment_token = current_token_global;
        if (!eat(lexer, PARSER_TOKEN_ASSIGN)) {
            node = parse_error("Expected '=' after identifier");
            panic_mode(lexer);
            return node;
        }

        ASTNode *expr_node = parse_expression(lexer);
        node = create_ast_node(AST_ASSIGNMENT, NULL, assignment_token);
        ASTNode *identifier_node = create_ast_node(AST_IDENTIFIER, var_name, var_name_token);
        identifier_node->parent = node;
        expr_node->parent = node;
        node->child = identifier_node;
        identifier_node->sibling = expr_node;
        if (expr_node->type == AST_ERROR) {
            return node;
        }
        check_semicolon(lexer, node);

    } else if (current_token_global->type == PARSER_TOKEN_SAY) {
        // Say instruction
        ParserToken *say_token = current_token_global;
        eat(lexer, PARSER_TOKEN_SAY);
        ASTNode *expr_node = parse_expression(lexer);
        node = create_ast_node(AST_SAY, NULL, say_token);
        expr_node->parent = node;
        node->child = expr_node;
        if (expr_node->type == AST_ERROR) {
            return node;
        }
        check_semicolon(lexer, node);

    } else {
        node = parse_error("Unknown statement");
        panic_mode(lexer);
    }

    return node;
}

// Parse an expression
ASTNode *parse_expression(Lexer *lexer) { // NOLINT(misc-no-recursion)
    ASTNode *node = parse_mulexp(lexer);
    if (node->type == AST_ERROR) {
        return node;
    }
    while (current_token_global->type == PARSER_TOKEN_PLUS || current_token_global->type == PARSER_TOKEN_MINUS) {
        char *op;
        ParserToken *op_token = current_token_global;
        if (current_token_global->type == PARSER_TOKEN_PLUS) {
            op = "+";
            eat(lexer, PARSER_TOKEN_PLUS);
        }
        else {
            op = "-";
            eat(lexer, PARSER_TOKEN_MINUS);
        }
        ASTNode *right = parse_mulexp(lexer);
        ASTNode *new_node = create_ast_node(AST_BINARY_OP, op, op_token);
        new_node->child = node;
        node->parent = new_node;
        right->parent = new_node;
        node->sibling = right;
        node = new_node;
        if (right->type == AST_ERROR) {
            return node;
        }
    }

    return node;
}

// Parse a multiplication expression
ASTNode *parse_mulexp(Lexer *lexer) { // NOLINT(misc-no-recursion)
    ASTNode *node = parse_term(lexer);
    if (node->type == AST_ERROR) {
        return node;
    }
    while (current_token_global->type == PARSER_TOKEN_MULTIPLY || current_token_global->type == PARSER_TOKEN_DIVIDE) {
        char *op;
        ParserToken *op_token = current_token_global;
        if (current_token_global->type == PARSER_TOKEN_MULTIPLY) {
            op = "*";
            eat(lexer, PARSER_TOKEN_MULTIPLY);
        }
        else {
            op = "/";
            eat(lexer, PARSER_TOKEN_DIVIDE);
        }
        ASTNode *right = parse_term(lexer);
        ASTNode *new_node = create_ast_node(AST_BINARY_OP, op, op_token);
        new_node->child = node;
        node->parent = new_node;
        right->parent = new_node;
        node->sibling = right;
        node = new_node;
        if (right->type == AST_ERROR) {
            return node;
        }
    }

    return node;
}

// Parse a term
ASTNode *parse_term(Lexer *lexer) { // NOLINT(misc-no-recursion)
    ASTNode *node = NULL;

    if (current_token_global->type == PARSER_TOKEN_LPAREN) {
        eat(lexer, PARSER_TOKEN_LPAREN);
        node = parse_expression(lexer);
        if (node->type == AST_ERROR) {
            return node;
        }
        if (!eat(lexer, PARSER_TOKEN_RPAREN)) {
            node = parse_error("Expected ')'");
            panic_mode(lexer);
        }
    }
    else if (current_token_global->type == PARSER_TOKEN_NUMBER) {
        node = create_ast_node(AST_NUMBER, current_token_global->value, current_token_global);
        eat(lexer, PARSER_TOKEN_NUMBER);
    } else if (current_token_global->type == PARSER_TOKEN_STRING) {
        node = create_ast_node(AST_STRING, current_token_global->value, current_token_global);
        eat(lexer, PARSER_TOKEN_STRING);
    } else if (current_token_global->type == PARSER_TOKEN_IDENTIFIER) {
        node = create_ast_node(AST_IDENTIFIER, current_token_global->value, current_token_global);
        eat(lexer, PARSER_TOKEN_IDENTIFIER);
    } else if (current_token_global->type == PARSER_TOKEN_UNTERMINATED_STRING) {
        node = parse_error("Unterminated string");
        current_token_global->type = PARSER_TOKEN_STRING;
        panic_mode(lexer);
    } else {
        node = parse_error("Unexpected token in expression");
        panic_mode(lexer);
    }

    return node;
}

/* Creates a new AST_ERROR node with an error message */
ASTNode *parse_error(const char *message) {
    ParserToken *error_token = current_token_global;
    ASTNode *error_node = create_ast_node(AST_ERROR, message, error_token);
    return error_node;
}

// AST functions
void print_ast(ASTNode *node, int indent) { // NOLINT(misc-no-recursion)
    if (node == NULL) return;

    int node_num = (int)(node->token?node->token->token_number:-1);
    int pos = (int)(node->token?node->token->pos:-1);
    int length = (int)(node->token?node->token->length:-1);

    switch (node->type) {
        case AST_PROGRAM:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Program (%d):\n", node_num);
            break;
        case AST_VARIABLE_DECL:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Variable Declaration (%d):\n", node_num);
            break;
        case AST_ASSIGNMENT:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Assignment (%d):\n", node_num);
            break;
        case AST_SAY:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Say (%d):\n", node_num);
            break;
        case AST_BINARY_OP:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Binary Operation (%d) '%s':\n", node_num, node->value);
            break;
        case AST_NUMBER:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Number (%d) '%s':\n", node_num, node->value);
            break;
        case AST_IDENTIFIER:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Identifier (%d) '%s':\n", node_num, node->value);
            break;
        case AST_STRING:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("String (%d) '%s':\n", node_num, node->value);
            break;
        case AST_ERROR:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Error (%d) '%s':\n", node_num, node->value);
            break;
        default:
            for (int i = 0; i < indent; i++) printf("  ");
            printf("Unknown AST Node (%d):\n", node_num);
            break;
    }

    // Print token information
    for (int i = 0; i < indent + 1; i++) printf("  ");
    printf("Token: %d, Pos: %d, Length: %d\n", node_num, pos, length);

    // Print children
    if (node->child != NULL) {
        print_ast(node->child, indent + 1);
    }

    // Print siblings
    if (node->sibling != NULL) {
        print_ast(node->sibling, indent);
    }
}

void free_ast(ASTNode *node) { // NOLINT(misc-no-recursion)
    if (node == NULL) return;
    if (node->child) free_ast(node->child);
    if (node->sibling) free_ast(node->sibling);
    if (node->value) free(node->value);
    free(node);
}

/* Panic mode error recovery */
void panic_mode(Lexer *lexer) {

    while (current_token_global->type != PARSER_TOKEN_EOF &&
            current_token_global->type != PARSER_TOKEN_SEMICOLON) {
        current_token_global = lexer_get_next_token(lexer);
    }

    if (current_token_global->type == PARSER_TOKEN_SEMICOLON) {
        current_token_global = lexer_get_next_token(lexer); /* Consume synchronizing token */
    }
}
