//
// DSLSyntax Parser
// This file is part of the DSL (Domain Specific Language) editor library.
// It provides functions and structures for a parser (server) to handle requests from a code editor (client)
// for syntax highlighting, error reporting, and other code editing features.
//
#include "dslsyntax_common.h"
#include "dslsyntax_parser.h"

/*
 * Load the Initial Content
 * This function sets the server/parser CodeBuffer object, and parses it.
 * It frees the initial load after setting the code buffer.
 */
void parser_load_initial_content(CodeBuffer *cb, InitialLoad *initial_load) {
    if (!cb || !initial_load) {
        fprintf(stderr, "Invalid arguments: CodeBuffer or InitialLoad is NULL.\n");
        exit(EXIT_FAILURE);
    }

    base_load_initial_content(cb, initial_load);
    // sleep to simulate some processing time
    usleep(1000000); // Sleep for 1000 milliseconds to simulate processing time
}