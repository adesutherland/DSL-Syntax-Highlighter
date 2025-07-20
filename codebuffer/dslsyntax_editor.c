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

/* Helper Function to Split Content into Lines.
 * content is utf8 or ascii, returned value is utf32 */
static void split_content_into_lines(char *content, size_t *no_lines, char32_t*** line_contents, size_t** line_lengths) {
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
    (*line_contents) = (char32_t**)malloc(sizeof(char32_t*) * lines);
    if (!(*line_contents)) {
        perror("Failed to allocate memory for lines");
        exit(EXIT_FAILURE);
    }

    /* Allocate memory for line lengths */
    (*line_lengths) = (size_t *)malloc(sizeof(size_t) * lines);
    if (!(*line_lengths)) {
        perror("Failed to allocate memory for line lengths");
        exit(EXIT_FAILURE);
    }

    /* Split the content */
    size_t current_line = 0;
    size_t start = 0;
    for (i = 0; i <= length; i++) {
        if (content[i] == '\0' || content[i] == '\n') {
            char32_t* utf32 = first_line_utf8_to_utf32(content + start, &(*line_lengths)[current_line]);
            (*line_contents)[current_line] = utf32;
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
    split_content_into_lines((char*)content, &(initial_load->line_count), &(initial_load->lines), &(initial_load->line_lengths));

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

    /* Copy the line lengths */
    copy->line_lengths = (size_t *)malloc(sizeof(size_t) * initial_load->line_count);
    if (!copy->line_lengths) {
        perror("Failed to allocate memory for line lengths copy");
        exit(EXIT_FAILURE);
    }
    memcpy(copy->line_lengths, initial_load->line_lengths, sizeof(size_t) * initial_load->line_count);

    /* Copy the lines */
    copy->lines = (char32_t **)malloc(sizeof(char32_t *) * copy->line_count);
    if (!copy->lines) {
        perror("Failed to allocate memory for lines copy");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < copy->line_count; i++) {
        size_t length = initial_load->line_lengths[i];
        copy->lines[i] = (char32_t *)malloc(sizeof(char32_t) * length);
        if (!copy->lines[i]) {
            perror("Failed to allocate memory for line copy");
            exit(EXIT_FAILURE);
        }
        memcpy(copy->lines[i], initial_load->lines[i], sizeof(char32_t) * length);
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
