#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "dslsyntax_common.h"
#include "dslsyntax_log.h"

/* Helper to setup a simple CodeBuffer with text */
CodeBuffer* setup_cb(const char *filename, const char *line_text) {
    CodeBuffer *cb = create_code_buffer(NULL, NULL);
    cb->unique_document_id = strdup(filename);
    cb->line_count = 1;
    cb->lines = calloc(1, sizeof(CodeBufferLine));
    cb->lines[0].length = strlen(line_text);
    cb->lines[0].characters = calloc(cb->lines[0].length + 1, sizeof(CodeBufferCharacter));
    for (size_t i = 0; i < cb->lines[0].length; i++) {
        cb->lines[0].characters[i].character[0] = line_text[i];
        cb->lines[0].characters[i].codepoints = 1;
        cb->lines[0].characters[i].token_type = LEXER_TOKEN;
    }
    cb->lines[0].characters[cb->lines[0].length].character[0] = '\0';
    cb->lines[0].characters[cb->lines[0].length].codepoints = 0;
    return cb;
}

void test_ep_learning_comprehensive() {
    printf("Testing EP Learning (Comprehensive - Unseeded)...\n");
    CodeBuffer *cb = setup_cb("test.toy", "say int x += 5; error_here");
    
    cb->parse_tree = calloc(1, sizeof(CB_ParseTree));
    cb->parse_tree->root = calloc(1, sizeof(CB_Node));
    cb->parse_tree->root->type = PARSE_TREE_FILE;
    cb->parse_tree->root->length = 26;

    CB_Node *kw1 = calloc(1, sizeof(CB_Node));
    kw1->type = LEXER_KEYWORD; kw1->pos = 0; kw1->length = 3; /* "say" */
    cb->parse_tree->root->child = kw1;

    CB_Node *kw2 = calloc(1, sizeof(CB_Node));
    kw2->type = LEXER_KEYWORD; kw2->pos = 4; kw2->length = 3; /* "int" */
    kw1->sibling = kw2;

    CB_Node *op1 = calloc(1, sizeof(CB_Node));
    op1->type = LEXER_OPERATOR; op1->pos = 10; op1->length = 2; /* "+=" */
    kw2->sibling = op1;

    CB_Node *err = calloc(1, sizeof(CB_Node));
    err->type = LEXER_TOKEN; err->pos = 16; err->length = 10; /* "error_here" */
    err->severity = CB_ERROR;
    op1->sibling = err;

    cb_learn_ep_rules(cb);
    assert(cb->ep_rules != NULL);
    assert(cb->ep_rules->keyword_count == 2);
    
    for (size_t i = 0; i < cb->ep_rules->keyword_count; i++) {
        assert(strcmp(cb->ep_rules->keywords[i], "error_here") != 0);
    }

    cb->line_count = 2;
    cb->lines = realloc(cb->lines, 2 * sizeof(CodeBufferLine));
    const char *text2 = "say int y += 10;";
    cb->lines[1].length = strlen(text2);
    cb->lines[1].characters = calloc(cb->lines[1].length + 1, sizeof(CodeBufferCharacter));
    for (size_t i = 0; i < cb->lines[1].length; i++) {
        cb->lines[1].characters[i].character[0] = text2[i];
        cb->lines[1].characters[i].codepoints = 1;
        cb->lines[1].characters[i].token_type = LEXER_TOKEN;
    }
    cb->lines[1].characters[cb->lines[1].length].character[0] = '\0';

    Transaction txn = {TRANSACTION_ADDLINE, 1, 0, (char*)text2, 0};
    cb_emergency_parse_transaction(cb, txn);

    assert(cb->lines[1].characters[0].token_type == LEXER_KEYWORD); /* "say" */
    assert(cb->lines[1].characters[4].token_type == LEXER_KEYWORD); /* "int" */
    assert(cb->lines[1].characters[10].token_type == LEXER_OPERATOR);
    
    printf("EP Learning (Comprehensive) passed!\n");
    free_code_buffer(cb);
}

void test_ep_preservation() {
    printf("Testing EP Preservation of Parsed Nodes...\n");
    /* Seed with quotes and keywords */
    CodeBuffer *cb = setup_cb("test.c", "say \"sss\"");
    cb->ep_rules = calloc(1, sizeof(EP_Rules));
    char **kw = malloc(sizeof(char*)); kw[0] = strdup("say");
    cb->ep_rules->keywords = kw; cb->ep_rules->keyword_count = 1;
    cb->ep_rules->string_quotes = strdup("\""); cb->ep_rules->string_quote_count = 1;

    /* Manually set up parsed nodes */
    for (int i = 0; i < 3; i++) {
        cb->lines[0].characters[i].node = (CB_Node*)0x1; /* Dummy pointer to mark as parsed */
        cb->lines[0].characters[i].token_type = LEXER_IDENTIFIER; /* The authoritative parser says it's an identifier, not a keyword */
    }
    for (int i = 4; i < 9; i++) {
        cb->lines[0].characters[i].node = (CB_Node*)0x1;
        cb->lines[0].characters[i].token_type = LEXER_IDENTIFIER; /* Erroneously parsed string as identifier */
    }

    /* Re-scan */
    cb_emergency_parse_transaction(cb, (Transaction){TRANSACTION_ADDCHARS, 0, 4, "", 0});
    
    /* Assert that EP did NOT override the parser's type */
    assert(cb->lines[0].characters[0].token_type == LEXER_IDENTIFIER);
    assert(cb->lines[0].characters[4].token_type == LEXER_IDENTIFIER);
    
    printf("EP Preservation test passed!\n");
    free_code_buffer(cb);
}

void test_ep_boundaries() {
    printf("Testing EP Word Boundaries...\n");
    CodeBuffer *cb = setup_cb("test.c", "sayx say");
    cb->ep_rules = calloc(1, sizeof(EP_Rules));
    char **kw = malloc(sizeof(char*)); kw[0] = strdup("say");
    cb->ep_rules->keywords = kw; cb->ep_rules->keyword_count = 1;

    cb_emergency_parse_transaction(cb, (Transaction){TRANSACTION_ADDCHARS, 0, 0, "", 0});
    
    /* 'sayx' should NOT be highlighted as a keyword */
    assert(cb->lines[0].characters[0].token_type != LEXER_KEYWORD);
    assert(cb->lines[0].characters[3].token_type != LEXER_KEYWORD);

    /* 'say' SHOULD be highlighted */
    assert(cb->lines[0].characters[5].token_type == LEXER_KEYWORD);
    
    printf("EP Boundaries test passed!\n");
    free_code_buffer(cb);
}

void test_ep_config_seeding() {
    printf("Testing EP Config Loading and Seeding...\n");
    const char *conf = "[.rexx]\nkeywords=say,pull\nline_comment=--\n\n[.md]\noperators=#,*\n";
    FILE *f = fopen("test_ep.conf", "w");
    fputs(conf, f);
    fclose(f);

    cb_load_ep_config("test_ep.conf");

    CodeBuffer *cb_rexx = setup_cb("script.rexx", "say 'hello' -- comment");
    cb_seed_ep_rules(cb_rexx, "script.rexx");
    
    assert(cb_rexx->lines[0].characters[0].token_type == LEXER_KEYWORD);
    assert(cb_rexx->lines[0].characters[12].token_type == LEXER_COMMENT);
    printf("Rexx seeding passed!\n");

    free_code_buffer(cb_rexx);
    unlink("test_ep.conf");
}

void test_ep_comment_extension() {
    printf("Testing EP Comment Extension...\n");
    CodeBuffer *cb = setup_cb("test.c", "/* comment */");
    cb->ep_rules = calloc(1, sizeof(EP_Rules));
    char **bc = malloc(sizeof(char*)); bc[0] = strdup("/*");
    cb->ep_rules->block_comment_starts = bc; cb->ep_rules->block_comment_count = 1;

    for (int i = 0; i < 13; i++) {
        cb->lines[0].characters[i].node = (CB_Node*)0x1;
        cb->lines[0].characters[i].token_type = LEXER_COMMENT;
    }

    /* Simulate inserting a space inside the comment at index 3 */
    Transaction txn = {TRANSACTION_ADDCHARS, 0, 3, " ", 0};
    cb_emergency_parse_transaction(cb, txn);

    /* The new space should inherit the comment type */
    assert(cb->lines[0].characters[3].token_type == LEXER_COMMENT);

    printf("EP Comment Extension test passed!\n");
    free_code_buffer(cb);
}

void test_ep_block_comment_learning() {
    printf("Testing EP Block Comment Learning...\n");
    CodeBuffer *cb = setup_cb("test.toy", "/* new block comment */");

    cb->parse_tree = calloc(1, sizeof(CB_ParseTree));
    cb->parse_tree->root = calloc(1, sizeof(CB_Node));
    cb->parse_tree->root->type = PARSE_TREE_FILE;
    cb->parse_tree->root->length = 23;

    CB_Node *comm = calloc(1, sizeof(CB_Node));
    comm->type = LEXER_COMMENT; comm->pos = 0; comm->length = 23; 
    cb->parse_tree->root->child = comm;

    cb_learn_ep_rules(cb);
    assert(cb->ep_rules != NULL);
    assert(cb->ep_rules->block_comment_count == 1);
    assert(strcmp(cb->ep_rules->block_comment_starts[0], "/*") == 0);
    assert(strcmp(cb->ep_rules->block_comment_ends[0], "*/") == 0);

    printf("EP Block Comment Learning test passed!\n");
    free_code_buffer(cb);
}

void test_ep_comma_list_and_escapes() {
    printf("Testing EP Comma List and Escapes...\n");
    const char *conf = "[.test]\nkeywords=a,b\noperators=+,-,\\,,*\nline_comment=//,#\n";
    cb_load_ep_config_from_string(conf);

    CodeBuffer *cb = setup_cb("file.test", "a + , // c");
    cb_seed_ep_rules(cb, "file.test");

    assert(cb->lines[0].characters[0].token_type == LEXER_KEYWORD); // 'a'
    assert(cb->lines[0].characters[2].token_type == LEXER_OPERATOR); // '+'
    assert(cb->lines[0].characters[4].token_type == LEXER_OPERATOR); // ','
    assert(cb->lines[0].characters[6].token_type == LEXER_COMMENT); // '// c'

    printf("EP Comma List and Escapes test passed!\n");
    free_code_buffer(cb);
}

void test_ep_greedy_numerics() {
    printf("Testing EP Greedy Numerics...\n");
    CodeBuffer *cb = setup_cb("test.c", "42 3.14 0xFF 100ULL");

    cb_emergency_parse_transaction(cb, (Transaction){TRANSACTION_ADDCHARS, 0, 0, "", 0});

    assert(cb->lines[0].characters[0].token_type == LEXER_NUMBER_LITERAL); // 4
    assert(cb->lines[0].characters[1].token_type == LEXER_NUMBER_LITERAL); // 2
    assert(cb->lines[0].characters[3].token_type == LEXER_NUMBER_LITERAL); // 3
    assert(cb->lines[0].characters[4].token_type == LEXER_NUMBER_LITERAL); // .
    assert(cb->lines[0].characters[6].token_type == LEXER_NUMBER_LITERAL); // 4
    assert(cb->lines[0].characters[8].token_type == LEXER_NUMBER_LITERAL); // 0
    assert(cb->lines[0].characters[9].token_type == LEXER_NUMBER_LITERAL); // x
    assert(cb->lines[0].characters[13].token_type == LEXER_NUMBER_LITERAL); // 1
    assert(cb->lines[0].characters[16].token_type == LEXER_NUMBER_LITERAL); // U

    printf("EP Greedy Numerics test passed!\n");
    free_code_buffer(cb);
}

void test_ep_ident_extra_chars() {
    printf("Testing EP Ident Extra Chars...\n");
    const char *conf = "[.test2]\nkeywords=if\nident_extra_chars=$\n";
    cb_load_ep_config_from_string(conf);

    CodeBuffer *cb = setup_cb("file.test2", "if $var");
    cb_seed_ep_rules(cb, "file.test2");

    assert(cb->lines[0].characters[0].token_type == LEXER_KEYWORD); // 'if'
    assert(cb->lines[0].characters[3].token_type == LEXER_IDENTIFIER); // '$'
    assert(cb->lines[0].characters[4].token_type == LEXER_IDENTIFIER); // 'v'

    printf("EP Ident Extra Chars test passed!\n");
    free_code_buffer(cb);
}

int main() {
    test_ep_learning_comprehensive();
    test_ep_block_comment_learning();
    test_ep_preservation();
    test_ep_boundaries();
    test_ep_comment_extension();
    test_ep_config_seeding();
    test_ep_comma_list_and_escapes();
    test_ep_greedy_numerics();
    test_ep_ident_extra_chars();

    printf("All enhanced EP tests passed successfully!\n");
    return 0;
}
