/* main.c */

#include <unistd.h> // For sleep function

#include "dslsyntax_common.h"
#include "dslsyntax_editor.h" // Editor library header
#include "parser_highlighter.h" // Toy parser highlighter header

// Function to print the code buffer with the attributes colourised using ansi escape codes
void print_code_buffer_with_attributes(CodeBuffer *cb);

// Function to apply a transaction and print the buffer
void test_apply_transaction(CodeBuffer *cb, TransactionType type, int pos_line, int pos_col, char *content, int count)
{
    int rc;
    Transaction txn;

    txn.type = type;
    txn.pos_line = pos_line; /* Zero-based index */
    txn.pos_col = pos_col;
    txn.content = content;
    txn.count = count;

    // Print the transaction being applied
    printf("Applying Transaction: Type %s, Line %d, Col %d, Content '%s', Count %d\n",
           transaction_type_to_string(type), pos_line, pos_col, content ? content : "NULL", count);

    if (!cb) {
        fprintf(stderr, "CodeBuffer is NULL\n");
        return;
    }
    // enter the critical section again
    rc = enter_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to enter critical section: %d\n", rc);
        exit( 1);
    }
    editor_apply_transaction(cb, txn);
    printf("Buffer After Transaction:\n");
    print_code_buffer_with_attributes(cb);
    printf("\n");

    // leave the critical section
    rc = exit_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to exit critical section: %d\n", rc);
        exit(1);
    }
}

// Function to print the code buffer with attributes
void test_print_cb(CodeBuffer *cb, const char *title) {
    // Enter a critical section to ensure thread safety
    int rc = enter_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to enter critical section: %d\n", rc);
        exit(1);
    }

    /* Print initial buffer */
    printf("%s\n", title ? title : "Code Buffer with Attributes:");
    print_code_buffer_with_attributes(cb);
    printf("\n");

    // leave the critical section
    rc = exit_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to exit critical section: %d\n", rc);
        exit(1);
    }
}

void print_delta(Delta *delta, const char *title) {
    printf("%s\n", title ? title : "Delta Information:");
    printf("Change Version: %d\n", (int)delta->change_version);
    for (size_t i = 0; i < delta->transaction_count; i++) {
        const char* trans = transaction_type_to_string(delta->transactions[i].type);
        if (delta->transactions[i].content) {
            printf("Transaction %zu: Type %s, Line %d, Col %d, Content '%s'\n",
                   i, trans, delta->transactions[i].pos_line,
                   delta->transactions[i].pos_col, delta->transactions[i].content);
        } else {
            printf("Transaction %zu: Type %s, Line %d, Col %d\n",
                   i, trans, delta->transactions[i].pos_line,
                   delta->transactions[i].pos_col);
        }
    }
    printf("\n");
}

int main() {
    CodeBuffer *editor_cb;
    CodeBuffer *parser_cb;
    const char *initial_content = "int x = 5;\nint y;\ny = x + 10;\nsay \"The value of y is:\";\nsay y;";
    Delta *delta = NULL;
    Transaction txn1;
    Transaction txn2;
    Transaction txn3;

    // Loop 100 times to simulate a long-running editor
    // And try to detect race conditions
    for (int i = 0; i < 1; i++) {

        editor_init(); // Initialize the editor side of the library

        // Create parser CodeBuffer
        parser_cb = create_code_buffer(0, toy_parser);

        // Create communication functions
        CommunicationFunctions *comm = create_inproc_communication_functions(parser_cb);

        /* Create editor CodeBuffer */
        editor_cb = create_code_buffer(comm,0);

        InitialLoad *initial = create_initial_load("doc1", initial_content);

        /* Set initial content - by the editor */
        load_initial_content(editor_cb, initial);

        // usleep(500000); // Sleep for 100 milliseconds

        test_print_cb(editor_cb, "Initial Buffer - emergency highlighting:");

        /* Perform a transaction: Add a new line */
        test_apply_transaction(editor_cb, TRANSACTION_ADDLINE, 5, 0, "say \"New line added.\"", 0);

        /* Perform a transaction: Add characters */
        test_apply_transaction(editor_cb, TRANSACTION_ADDCHARS, 0, 4, "const ", 0);

        /* Perform a transaction: Delete characters */
        /* Starting at 'x' in line 2, column 4 and deleting 1 character ('x') */
        test_apply_transaction(editor_cb, TRANSACTION_DELETECHARS, 2, 4, NULL, 1);

        /* Wait for Parse complete */
        if (wait_for_parse_complete_event() != 0) {
            fprintf(stderr, "Error waiting for parse complete event\n");
            return 1;
        }

        test_print_cb(editor_cb, "Buffer After Parse Complete:");

        process_delta(editor_cb);

        // Wait for the parser to process the delta
        if (wait_for_parse_complete_event() != 0) {
            fprintf(stderr, "Error waiting for parse complete event after delta processing\n");
            return 1;
        }

        test_print_cb(editor_cb, "Buffer After Delta Processing:");

        /* Free the CodeBuffer */
        free_code_buffer(editor_cb);

        /* Free the parser CodeBuffer */
        free_code_buffer(parser_cb);

        /* Free the communication functions */
        free_inproc_communication_functions(comm);

        /* Free the editor library */
        editor_free();
    }
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
        for (j = 0; j < cb->lines[i].length; j++) {
            // Print the escape code for the token
            if (cb->lines[i].characters[j].token_type != last_type || cb->lines[i].characters[j].severity != last_severity) {
                last_type = cb->lines[i].characters[j].token_type;
                last_severity = cb->lines[i].characters[j].severity;
                printf("%s%s",severity_to_ansi_escape(last_severity), attribute_to_ansi_escape(last_type));
            }
            // Print the character
            // Is the utf32 character invalid or unprintable
            if (cb->lines[i].characters[j].character[0] < 32 || cb->lines[i].characters[j].character[0] > 126) {
                // Print a placeholder for unprintable characters
                utf8_char[0] = '?';
                utf8_char[1] = 0;
            } else {
                // Convert the utf32 character to utf8
                size_t s = utf32_to_utf8_char(cb->lines[i].characters[j].character[0], utf8_char, sizeof(utf8_char));
                utf8_char[s] = 0; // Null-terminate the string
            }
            printf("%s", utf8_char);
        }
        printf("\n");
    }
    // Reset the escape code
    printf("\033[30m\033[0m");
    printf("Highest Severity: %d\n", cb->highest_severity);
}
