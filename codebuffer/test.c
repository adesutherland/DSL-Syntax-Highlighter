/* main.c */

#include <unistd.h> // For sleep function

#include "token_buffer.h"
#include "parser_highlighter.h" // Toy parser highlighter header

// Function to print the code buffer with the attributes colourised using ansi escape codes
void print_code_buffer_with_attributes(CodeBuffer *cb);

int main() {
    CodeBuffer *editor_cb;
    CodeBuffer *parser_cb;
    const char *initial_content = "int x = 5;\nint y;\ny = x + 10;\nsay \"The value of y is:\";\nsay y;";
    Delta *delta = NULL;
    Transaction txn1;
    Transaction txn2;
    Transaction txn3;

    editor_init(); // Initialize the editor side of the library

    // Create parser CodeBuffer
    parser_cb = create_code_buffer(0, toy_parser);

    // Create communication functions
    CommunicationFunctions *comm = create_inproc_communication_functions(parser_cb);

    /* Create editor CodeBuffer */
    editor_cb = create_code_buffer(comm,0);


    // Loop this section 100 times to simulate a long-running editor
    // And try to detect race conditions
    for (int i = 0; i < 100; i++) {
        // Simulate some work in the editor
        usleep(500000); // Sleep for 100 milliseconds

        InitialLoad *initial = create_initial_load("doc1", initial_content);

        /* Set initial content - by the editor */
        load_initial_content(editor_cb, initial);

        usleep(500000); // Sleep for 100 milliseconds


        // Enter critical section to ensure thread safety
        int rc = enter_codeblock_critical_section();
        if (rc != 0) {
            fprintf(stderr, "Failed to enter critical section: %d\n", rc);
            return 1;
        }

        /* Print initial buffer */
        printf("Initial Buffer - emergency highlighting:\n");
        print_code_buffer_with_attributes(editor_cb);
        printf("\n");

        // leaave critical section
        rc = exit_codeblock_critical_section();
        if (rc != 0) {
            fprintf(stderr, "Failed to exit critical section: %d\n", rc);
            return 1;
        }

        //sleep(2); // Simulate some delay for demonstration purposes
        usleep(500000); // Sleep for 100 milliseconds

        /* Wait for Parse complete */
        if (wait_for_parse_complete_event() != 0) {
            fprintf(stderr, "Error waiting for parse complete event\n");
            return 1;
        }

        // enter critical section again
        rc = enter_codeblock_critical_section();
        if (rc != 0) {
            fprintf(stderr, "Failed to enter critical section: %d\n", rc);
            return 1;
        }
        printf("Initial Buffer final highlighting:\n");
        print_code_buffer_with_attributes(editor_cb);
        printf("\n");

        // leave critical section
        rc = exit_codeblock_critical_section();
        if (rc != 0) {
            fprintf(stderr, "Failed to exit critical section: %d\n", rc);
            return 1;
        }
    }

    /* Free the CodeBuffer */
    free_code_buffer(editor_cb);

    /* Free the parser CodeBuffer */
    free_code_buffer(parser_cb);

    /* Free the editor library */
    editor_free();


return 0;

    /* Perform a transaction: Add a new line */
    txn1.type = TRANSACTION_ADDLINE;
    txn1.pos_line = 5; /* Zero-based index */
    txn1.pos_col = 0;
    txn1.content = "say \"New line added.\"";
    txn1.count = 0;
    apply_transaction(editor_cb, txn1);

    /* Perform a transaction: Add characters */
    txn2.type = TRANSACTION_ADDCHARS;
    txn2.pos_line = 0;
    txn2.pos_col = 4; /* After 'int ' */
    txn2.content = "const ";
    txn2.count = 0;
    apply_transaction(editor_cb, txn2);

    /* Perform a transaction: Delete characters */
    txn3.type = TRANSACTION_DELETECHARS;
    txn3.pos_line = 2;
    txn3.pos_col = 4; /* Starting at 'x' */
    txn3.count = 1;    /* Delete 'x' */
    txn3.content = NULL;
    apply_transaction(editor_cb, txn3);

    /* Take a snapshot and get the delta */
    delta = snapshot_and_get_delta(editor_cb);
    if (delta) {
        printf("Delta after transactions:\n");
        printf("Change Version: %d\n", (int)delta->change_version);
        printf("Unique Document ID: %s\n", delta->unique_document_id);
        for (size_t i = 0; i < delta->transaction_count; i++) {
            if (delta->transactions[i].content) {
                printf("Transaction %zu: Type %d, Line %d, Col %d, Content '%s'\n",
                       i, delta->transactions[i].type, delta->transactions[i].pos_line,
                       delta->transactions[i].pos_col, delta->transactions[i].content);
            } else {
                printf("Transaction %zu: Type %d, Line %d, Col %d\n",
                       i, delta->transactions[i].type, delta->transactions[i].pos_line,
                       delta->transactions[i].pos_col);
            }
        }
        printf("\n");
    }

    /* Print updated buffer */
    printf("Updated Buffer:\n");
    print_code_buffer_with_attributes(editor_cb);
    printf("\n");

    /* Print initial parser buffer */
    printf("Initial Parser Buffer:\n");
    print_code_buffer_with_attributes(parser_cb);
    printf("\n");

    /* Replay the delta to the parser */
    replay_delta(parser_cb, delta);

    /* Print buffer after replaying delta */
    printf("Parser Buffer after replaying delta:\n");
    print_code_buffer_with_attributes(parser_cb);
    printf("\n");

    // Example Toy Parser
    toy_parser(parser_cb);

    // Print the editor CodeBuffer with syntax highlighting
    printf("Editor CodeBuffer with Syntax Highlighting:\n");
    print_code_buffer_with_attributes(editor_cb);

    /* Free the delta */
    free_delta(delta);

    /* Free the CodeBuffer */
    free_code_buffer(editor_cb);

    /* Free the parser CodeBuffer */
    free_code_buffer(parser_cb);

    /* Free the editor library */
    editor_free();

    return 0;
}

// Utility to convert severity to ansi escape code - a yellow or red background
char *severity_to_ansi_escape(char severity) {
    switch (severity) {
        case CB_ERROR:
            return "\033[41m"; // Red background for errors
        case CB_WARNING:
            return "\033[43m"; // Yellow background for warnings
        case CB_INFORMATION:
            return "\033[42m"; // Green background for info
        default:
            return "\033[0m";  // Default no background
    }
}

// Utility to convert attribute to ansi escape code
char *attribute_to_ansi_escape(char token_type) {
    switch (token_type) {
        //  The CB_NodeType mapping
        case LEXER_UNKNOWN:
            return "\033[31m"; // Red
        case LEXER_COMMENT:
            return "\033[32m"; // Green
        case LEXER_STRING_LITERAL:
        case LEXER_NUMBER_LITERAL:
            return "\033[34m"; // Blue
        case LEXER_KEYWORD:
            return "\033[35m"; // Magenta
        case LEXER_OPERATOR:
        case LEXER_OPERATOR_ARITHMETIC:
        case LEXER_OPERATOR_ASSIGN:
        case LEXER_OPERATOR_LOGICAL:
            return "\033[36m"; // Cyan
        case LEXER_IDENTIFIER:
            return "\033[33m"; // Yellow
        default:
            // Default to black
            return "\033[30m"; // Black
    }
}

// Function to print the code buffer with the attributes colourised using ansi escape codes
void print_code_buffer_with_attributes(CodeBuffer *cb) {
    size_t i,j;
    char utf8_char[5]; // Buffer for utf8 character

    if (!cb) return;

    printf("CodeBuffer (Version: %d):\n", (int)cb->change_version);

    // Print each line with its attributes
    // Set the default token to black
    char last_type = LEXER_TOKEN;
    char last_severity = CB_NONE;
    // Print the escape code for the default token
    printf("%s",attribute_to_ansi_escape(last_type));
    for (i = 0; i < cb->line_count; i++) {
        for (j = 0; j < cb->line_lengths[i]; j++) {
            // Print the escape code for the token
            if (cb->attributes && (cb->attributes[i][j].token_type != last_type || cb->attributes[i][j].severity != last_severity)) {
                last_type = cb->attributes[i][j].token_type;
                last_severity = cb->attributes[i][j].severity;
                printf("%s%s",severity_to_ansi_escape(last_severity), attribute_to_ansi_escape(last_type));
            }
            // Print the character
            size_t s = utf32_to_utf8_char(cb->lines[i][j], utf8_char, sizeof(utf8_char));
            utf8_char[s] = 0;
            printf("%s", utf8_char);
        }
        printf("\n");
    }
    // Reset the escape code
    printf("\033[30m\033[0m");
    printf("Highest Severity: %d\n", cb->highest_severity);
}
