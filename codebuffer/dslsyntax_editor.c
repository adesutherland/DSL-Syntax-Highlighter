//
// DSLSyntax Editor
// This file is part of the DSL (Domain Specific Language) editor library.
// It provides functions and structures for a code editor (client) to interact with a parser (server)
// for syntax highlighting, error reporting, and other code editing features.
//
#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"

/* Function to initialise the editor side of the library */
void editor_init() {
    int rc = init_parser_thread_utils();
    if (rc != 0) {
        fprintf(stderr, "Failed to initialize thread utils for editor: %d\n", rc);
        exit(EXIT_FAILURE);
    }
}

/* Function to free the editor side of the library */
void editor_free() {
    destroy_thread_utils();
}

/* Utility to convert the first line of a null terminated utf8 or ascii string to */
/* TODO - Does not handle grapheme clusters */
/* Newline is not included in the output */
/* Returns 0 on success, 1 on failure (e.g., memory allocation failure) */
int first_line_utf8_to_line(const char* utf8_string, CodeBufferLine* line) {
    if (!utf8_string) {
        return 1; // Handle NULL input
    }

    // First pass: Calculate the number of UTF-32 code points.
    size_t utf32_length = 0;
    const char* temp_utf8_ptr = utf8_string;
    utf8_int32_t  decoded_code_point; // Variable to hold the decoded code point

    while (*temp_utf8_ptr && *temp_utf8_ptr != '\n') {
        // utf8codepoint() decodes the next code point and advances the pointer.
        temp_utf8_ptr = utf8codepoint(temp_utf8_ptr, &decoded_code_point);
        (void)decoded_code_point; // Suppress unused variable warning if needed
        utf32_length++;
    }

    // Allocate memory for the UTF-32 string
    line->characters = (CodeBufferCharacter*)malloc((utf32_length + 1) * sizeof(CodeBufferCharacter));
    if (!line) {
        perror("Failed to allocate memory for CodeBufferLine");
        return 1; // Indicate allocation failure
    }

    // Second pass: Decode and copy the UTF-32 code points.
    size_t utf32_pos = 0;
    temp_utf8_ptr = utf8_string; // Reset pointer to the beginning

    while (*temp_utf8_ptr && *temp_utf8_ptr != '\n') {
        // Decode the next code point and store it.
        temp_utf8_ptr = utf8codepoint(temp_utf8_ptr, &decoded_code_point);
        line->characters[utf32_pos].character[0] = decoded_code_point;
        line->characters[utf32_pos].character[1] = 0;
        line->characters[utf32_pos].codepoints = 1;
        line->characters[utf32_pos].token_type = LEXER_TOKEN; // Default token type
        line->characters[utf32_pos].severity = CB_NONE; // Default severity
        line->characters[utf32_pos].subtree_type = 0; // No subtree type by default
        line->characters[utf32_pos].subtree_lines = 0; // No subtree lines by default
        line->characters[utf32_pos].node = NULL; // No parser node by default
        utf32_pos++;
    }
    // "Null-terminate" the line
    line->characters[utf32_pos].character[0] = 0; // Null-terminate the last character
    line->characters[utf32_pos].codepoints = 0;
    line->characters[utf32_pos].token_type = LEXER_WHITESPACE; // The last character is whitespace
    line->characters[utf32_pos].severity = CB_NONE; // Default severity for the last character
    line->characters[utf32_pos].subtree_type = 0; // No subtree type for the last character
    line->characters[utf32_pos].subtree_lines = 0; // No subtree lines for the last character
    line->characters[utf32_pos].node = NULL; // No parser node for the last character

    // Set the length of the line
    line->length = utf32_length;

    return 0; // Success
}

/* Helper Function to Split Content into Lines.
 * content is utf8 or ascii, returned value is utf32 */
static void split_content_into_lines(char *content, size_t *no_lines, CodeBufferLine** line_contents) {
    size_t lines = 1;
    size_t i;
    size_t length = strlen(content);

    /* Count the number of lines */
    for (i = 0; i < length; i++) {
        if (content[i] == '\n') {
            lines++;
        }
    }

    /* Allocate memory for lines */
    (*line_contents) = (CodeBufferLine*)malloc(sizeof(CodeBufferLine) * lines);
    if (!(*line_contents)) {
        perror("Failed to allocate memory for lines");
        exit(EXIT_FAILURE);
    }

    /* Split the content */
    size_t current_line = 0;
    size_t start = 0;
    for (i = 0; i <= length; i++) {
        if (content[i] == '\0' || content[i] == '\n') {
            first_line_utf8_to_line(content + start, &(*line_contents)[current_line]);
            current_line++;
            start = i + 1;
        }
    }

    *no_lines = lines;
}

/*
 * Setting Initial Content
 * Function to create an initial load from a source string.
 * It is freed when it is used to apply the initial load with load_initial_content()
 */
InitialLoad* create_initial_load(const char *unique_document_id, const char *content) {
    InitialLoad *initial_load = (InitialLoad *)malloc(sizeof(InitialLoad));
    if (!initial_load) {
        perror("Failed to allocate memory for InitialLoad");
        exit(EXIT_FAILURE);
    }

    initial_load->unique_document_id = strdup(unique_document_id);
    if (!initial_load->unique_document_id) {
        perror("Failed to allocate memory for unique_document_id");
        exit(EXIT_FAILURE);
    }

    /* Split content into lines */
    split_content_into_lines((char*)content, &(initial_load->line_count), &(initial_load->lines));

    initial_load->change_version = 0;

    return initial_load;
}

/* Data Structure for the load_initial_content thread */
typedef struct {
    CodeBuffer *code_buffer;
    InitialLoad *initial_load;
} InitialLoadThreadData;


/* Thread that loads the initial content */
static void* load_initial_content_thread(void *arg) {
    InitialLoadThreadData *data = (InitialLoadThreadData *)arg;

    /* Send the initial load to the parser */
    CB_ParseTree *result = data->code_buffer->communication_functions->send_initial_load(data->code_buffer->communication_functions, data->initial_load);
// sleep(2); // Simulate some delay for the parser to process the initial load

    /* Enter the critical section */
    int rc = enter_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to enter critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Set the parse tree in the code buffer */
    if (data->code_buffer->parse_tree) {
        /* Free the existing parse tree */
        cb_free_token_buffer(data->code_buffer->parse_tree);
    }
    data->code_buffer->parse_tree = result;

    if (data->code_buffer->transaction_count > 0) {
        /* Reset to the snapshot */
        copy_snapshot_to_codebuffer(data->code_buffer);
    }

    highlight_syntax(data->code_buffer);

    /* Apply the parse result to the code buffer */
    if (data->code_buffer->transaction_count > 0) {
        /* Replay the transaction since the snapshot */
        int i;
        for (i = 0; i < data->code_buffer->transaction_count; i++) {
            base_apply_transaction(data->code_buffer, data->code_buffer->transactions[i]);
        }
    }

    // Signal the parse complete event
    rc = raise_parse_complete_event();
    if (rc != 0) {
        fprintf(stderr, "Failed to set parse complete event: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Exit the critical section */
    rc = exit_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to exit critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Free the arg structure */
    free(arg);

    return NULL;
}

/* Common helper function to do a deep copy of an InitialLoad */
static InitialLoad* copy_initial_load(InitialLoad *initial_load) {
    if (!initial_load) return NULL;

    InitialLoad *copy = (InitialLoad *)malloc(sizeof(InitialLoad));
    if (!copy) {
        perror("Failed to allocate memory for InitialLoad copy");
        exit(EXIT_FAILURE);
    }

    copy->line_count = initial_load->line_count;
    copy->change_version = initial_load->change_version;

    /* Copy the unique_document_id */
    copy->unique_document_id = strdup(initial_load->unique_document_id);
    if (!copy->unique_document_id) {
        perror("Failed to allocate memory for unique_document_id copy");
        exit(EXIT_FAILURE);
    }

    /* Copy the lines */
    copy->lines = (CodeBufferLine *)malloc(sizeof(CodeBufferLine) * copy->line_count);
    if (!copy->lines) {
        perror("Failed to allocate memory for lines copy");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < copy->line_count; i++) {
        size_t length = initial_load->lines[i].length;
        copy->lines[i].characters = (CodeBufferCharacter *)malloc(sizeof(CodeBufferCharacter) * (length + 1));
        if (!copy->lines[i].characters) {
            perror("Failed to allocate memory for line copy");
            exit(EXIT_FAILURE);
        }
        memcpy(copy->lines[i].characters, initial_load->lines[i].characters, sizeof(CodeBufferCharacter) * (length + 1));
        copy->lines[i].length = length;
    }

    return copy;
}


/*
 * Load the Initial Content
 * This function sets the local CodeBuffer object, after which the codeblock
 * can be used by the editor.
 * It frees the initial load after setting the code buffer.
 * It calls the communication function to send the initial load to the parser
 * The results of which will be applied to the code buffer when it arrives.
 */
void load_initial_content(CodeBuffer *cb, InitialLoad *initial_load) {
    int rc;
    if (!cb) {
        // Panic
        fprintf(stderr, "PANIC: CodeBuffer is NULL. Cannot load initial content.\n");
        exit(EXIT_FAILURE);
    }

    /* Enter the critical section */
    rc = enter_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to enter critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    if (editor_is_parsing_thread_active()) {
        // PANIC
        fprintf(stderr, "PANIC: Parsing thread is already active. Cannot load initial content.\n");
        exit(EXIT_FAILURE);
    }

    // Reset the parse_complete_event
    rc = reset_parse_complete_event();
    if (rc != 0) {
        fprintf(stderr, "Failed to reset parse complete event: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Send the initial load */
    /* Duplicate the initial load to avoid freeing it prematurely */
    InitialLoad *initial_load_copy = copy_initial_load(initial_load);

    /* Malloc the thread data */
    InitialLoadThreadData *arg = (InitialLoadThreadData *)malloc(sizeof(InitialLoadThreadData));
    if (!arg) {
        perror("Failed to allocate memory for InitialLoadThreadData");
        exit(EXIT_FAILURE);
    }

    /* Set the thread data */
    arg->code_buffer = cb;
    arg->initial_load = initial_load_copy;
    rc = launch_parser_thread(load_initial_content_thread, arg);
    if (rc != 0) {
        fprintf(stderr, "Failed to launch thread for initial load: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Call Base functionality to Load the Initial Content
     * This sets the local CodeBuffer object, after which the codeblock
     * can be used. It frees the initial load after setting the code buffer.
     */
    base_load_initial_content(cb, initial_load);

    /* Set the snapshot of the content */
    snapshot(cb);

    // Highlight the syntax of the editor CodeBuffer
    highlight_syntax(cb);

    /* Exit the critical section */
    rc = exit_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to exit critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }
}


/* Data Structure for process_delta_thread() */
typedef struct {
    CodeBuffer *code_buffer;
    Delta *delta;
} ProcessDeltaThreadData;


/* Thread that processes deltas and parses result */
static void* process_delta_thread(void *arg) {
    ProcessDeltaThreadData *data = (ProcessDeltaThreadData *)arg;

    /* Send the delta to the parser */
    CB_ParseTree *result = data->code_buffer->communication_functions->send_delta(data->code_buffer->communication_functions, data->delta);

    /* Enter the critical section */
    int rc = enter_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to enter critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Free the delta */
    if (data->delta) {
        free_delta(data->delta);
        data->delta = NULL;
    }

    /* Set the parse tree in the code buffer */
    if (data->code_buffer->parse_tree) {
        /* Free the existing parse tree */
        cb_free_token_buffer(data->code_buffer->parse_tree);
    }
    data->code_buffer->parse_tree = result;

    if (data->code_buffer->transaction_count > 0) {
        /* Reset to the snapshot */
        copy_snapshot_to_codebuffer(data->code_buffer);
    }

    highlight_syntax(data->code_buffer);

    /* Apply the parse result to the code buffer */
    if (data->code_buffer->transaction_count > 0) {
        /* Replay the transaction since the snapshot */
        int i;
        for (i = 0; i < data->code_buffer->transaction_count; i++) {
            base_apply_transaction(data->code_buffer, data->code_buffer->transactions[i]);
        }
    }

    // Signal the parse complete event
    rc = raise_parse_complete_event();
    if (rc != 0) {
        fprintf(stderr, "Failed to set parse complete event: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Exit the critical section */
    rc = exit_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to exit critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Free the arg structure */
    free(arg);

    return NULL;
}

/*
 * Creates and processes the delta and sends it to the parser.
 *
 * It calls the communication function to send the initial load to the parser
 * The results of which will be applied to the code buffer when it arrives.
 */
void process_delta(CodeBuffer *cb) {
    int rc;
    if (!cb) {
        // Panic
        fprintf(stderr, "PANIC: CodeBuffer is NULL. Cannot load initial content.\n");
        exit(EXIT_FAILURE);
    }

    /* Enter the critical section */
    rc = enter_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to enter critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    if (editor_is_parsing_thread_active()) {
        // PANIC
        fprintf(stderr, "PANIC: Parsing thread is already active. Cannot load initial content.\n");
        exit(EXIT_FAILURE);
    }

    // Reset the parse_complete_event
    rc = reset_parse_complete_event();
    if (rc != 0) {
        fprintf(stderr, "Failed to reset parse complete event: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    Delta *delta = snapshot_and_get_delta(cb);
    if (!delta) {
        fprintf(stderr, "Failed to create delta from code buffer\n");
        exit(EXIT_FAILURE);
    }
    if (delta->transaction_count == 0) {
        fprintf(stderr, "No transactions to process in delta\n");
        free_delta(delta);
        exit(EXIT_FAILURE); // TODO - this is really an acceptable NOP condition
    }

    /* Send the delta */

    /* Malloc the thread data */
    ProcessDeltaThreadData *arg = (ProcessDeltaThreadData *)malloc(sizeof(ProcessDeltaThreadData));
    if (!arg) {
        perror("Failed to allocate memory for ProcessDeltaThreadData");
        exit(EXIT_FAILURE);
    }

    /* Set the thread data */
    arg->code_buffer = cb;
    arg->delta = delta;
    rc = launch_parser_thread(process_delta_thread, arg);
    if (rc != 0) {
        fprintf(stderr, "Failed to launch thread for process_delta: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* Exit the critical section */
    rc = exit_codeblock_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to exit critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }
}
