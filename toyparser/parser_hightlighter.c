//
// Created by Adrian Sutherland on 13/10/2024.
//
#include <string.h>
#include "parser_highlighter.h"
#include "parser.h"

void ast_to_token_buffer_worker(ASTNode *node, CB_ParseTree *tb);

// The actual toy parser function
void toy_parser(CodeBuffer *codeBuffer) {
    char *source_code = get_code_buffer_source(codeBuffer);

    // Initialize lexer
    head = 0;
    Lexer lexer = {source_code, 0, source_code[0], 1, 0};

    // Parse the program
    ASTNode *ast = parse_program(&lexer);

    // Print the AST
    // print_ast(ast, 0);

    // Convert AST to CB_NodeBuffer
    CB_ParseTree *tb = cb_create_token_buffer();
    ast_to_token_buffer(ast, tb, codeBuffer);

    // Free parser resources
    free_ast(ast);
    free_token_list();
    free(source_code);

    // Print the CB_ParseTree
    // cb_print_token_buffer(codeBuffer, tb);

    // For debugging - sleep to simulate some processing time to simulate a slow parser
//    usleep(1000000); // Sleep for 1000 milliseconds to simulate processing time

    codeBuffer->parse_tree = tb; // Set the parse tree in the editor CodeBuffer
}

void highlight_init(char* source_code, CodeBuffer **cb, CB_ParseTree **tb) {

    if (*cb == NULL) {
        // Panic
        fprintf(stderr, "PANIC: CodeBuffer is NULL in highlight_init\n");
    }

    // Initialize lexer
    head = 0;
    Lexer lexer = {source_code, 0, source_code[0], 1, 0};

    // Parse the program
    ASTNode *ast = parse_program(&lexer);

    // Print the AST
    // print_ast(ast, 0);

    // Convert AST to CB_NodeBuffer
    *tb = cb_create_token_buffer();
    ast_to_token_buffer(ast, *tb, *cb);

    // Free parser resources
    free_ast(ast);
    free_token_list();
}

/*
 * Helper function to map ASTType (type) to CB_NodeType (returned by the function)
 *
 * The function should return the token type for the AST node - e.g., PARSE_TREE_EXPR
 * or LEXER_UNKNOWN if the node should be treated as a leaf node
 */
static CB_NodeType map_ast_to_token_type(ASTType type) {
    switch (type) {
        case AST_PROGRAM:
            return PARSE_TREE;

        case AST_VARIABLE_DECL:
        case AST_ASSIGNMENT:
        case AST_SAY:
            return PARSE_TREE_STATEMENT;

        case AST_BINARY_OP:
            return PARSE_TREE_EXPR;

        default:
            return LEXER_UNKNOWN;
    }
}

/* Helper function to map ParserTokenType to CB_NodeType */
static CB_NodeType map_parser_to_token_type(ParserTokenType type) {
    switch (type) {
        case PARSER_TOKEN_WHITESPACE:
            return LEXER_WHITESPACE;
        case PARSER_TOKEN_COMMENT:
            return LEXER_COMMENT;
        case PARSER_TOKEN_INT:
            return LEXER_KEYWORD;
        case PARSER_TOKEN_IDENTIFIER:
            return LEXER_IDENTIFIER;
        case PARSER_TOKEN_NUMBER:
            return LEXER_NUMBER_LITERAL;
        case PARSER_TOKEN_STRING:
            return LEXER_STRING_LITERAL;
        case PARSER_TOKEN_SAY:
            return LEXER_KEYWORD;
        case PARSER_TOKEN_ASSIGN:
            return LEXER_OPERATOR_ASSIGN;
        case PARSER_TOKEN_SEMICOLON:
            return LEXER_STATEMENT_SEPARATOR;
        case PARSER_TOKEN_LPAREN:
            return LEXER_LH_EXPR;
        case PARSER_TOKEN_RPAREN:
            return LEXER_RH_EXPR;
        case PARSER_TOKEN_PLUS:
        case PARSER_TOKEN_MINUS:
        case PARSER_TOKEN_MULTIPLY:
        case PARSER_TOKEN_DIVIDE:
            return LEXER_OPERATOR_ARITHMETIC;
        case PARSER_TOKEN_UNTERMINATED_STRING:
            return LEXER_STRING_LITERAL; /* Treat as string literal - error will be added */
        default:
            return LEXER_UNKNOWN;
    }
}


/* The callback function to generate/lookup missing tokens based on position and length */
/* This function should return a CB_Node for the given position and length, or null if no token is available */
/* The length of the returned token should be the same or less than the requested length */
CB_Node ast_get_token_callback(__attribute__((unused)) void *user_data, size_t pos, size_t length, __attribute__((unused))CodeBufferCharacter* token_chars) {
    /* Get the token at the given position */
    ParserToken* parser_token = get_token_by_pos(pos);
    if (parser_token->type == PARSER_TOKEN_EOF) {
        return cb_create_node(LEXER_EOF, pos, 1);
    }

    CB_NodeType tree_type = map_parser_to_token_type(parser_token->type);

    CB_Node token = cb_create_node(tree_type, pos, parser_token->length);

    if (token.length == 0) {
        // PANIC
        fprintf(stderr, "PANIC: Invalid token length in ast_get_token_callback\n");
        exit(1);
    }
    if (token.length > length) {
        // PANIC
        fprintf(stderr, "PANIC: Invalid token length in ast_get_token_callback\n");
        exit(1);
    }

    //CB_Node node = cb_default_get_token_callback(NULL, pos, length, token_chars);
    // todo
    return token;
}

/* Function to convert AST to CB_NodeBuffer */
void ast_to_token_buffer(ASTNode *node, CB_ParseTree *tb, CodeBuffer *cb) {
    if (node == NULL || tb == NULL) {
        // Panic
        fprintf(stderr, "PANIC: Invalid node or token buffer in ast_to_token_buffer\n");
        exit(1);
    }

    /* Convert AST to CB_NodeBuffer */
    ast_to_token_buffer_worker(node, tb);

    /* Sort the tokens */
    cb_order_tree(tb);

    /* Add any remaining tokens that were skipped */
    cb_add_missing_tokens(tb, cb, ast_get_token_callback, NULL);

    /* Tweak the positions of the tokens */
    cb_tweak_tree_positions(tb);

    /* Validate the tree */
    cb_validate_tree(tb);
}

/* Recursive Worker Function to convert AST to CB_NodeBuffer */
void ast_to_token_buffer_worker(ASTNode *node, CB_ParseTree *tb) { // NOLINT(misc-no-recursion)
    if (node == NULL || tb == NULL) return;

    CB_NodeType tree_type = map_ast_to_token_type(node->type);

    if (tree_type != LEXER_UNKNOWN) {
        // Add AST Tree
        CB_Node tree = cb_create_node(tree_type, 0, 0);
        cb_add_child_node(tb, tree);
        cb_set_current_parent_to_last_node(tb);
    }

    char *error_message = NULL;
    if (node->type == AST_ERROR) {
        error_message = node->value;
    }

    if (node->token != NULL) {
        ParserToken *parser_token = node->token;

        CB_Node token = cb_create_node(map_parser_to_token_type(parser_token->type),
                                       (int)parser_token->pos, (int)parser_token->length);

        // Unknown token type
        if (error_message != NULL) {
            token.message = error_message;
            token.severity = CB_ERROR;
        } else if (token.type == LEXER_UNKNOWN) {
            token.message = "Invalid Character(s)";
            token.severity = CB_ERROR;
        }

        // Add the token to the buffer
        cb_add_child_node(tb, token);
    }

    // Process children
    if (node->child != NULL) {
        ast_to_token_buffer_worker(node->child, tb);
    }

    if (tree_type != LEXER_UNKNOWN) {
        // Exit the subtree
        cb_set_current_parent_to_grandparent(tb);
    }

    // Process siblings
    if (node->sibling != NULL) {
        ast_to_token_buffer_worker(node->sibling, tb);
    }
}
