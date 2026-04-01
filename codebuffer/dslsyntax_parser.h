//
// DSL Syntax Parser
// This header file defines the DSL syntax parser, which is a part of the
// DSL (Domain Specific Language) editor library. It provides functions and
// structures for a parser (server) to handles requests from a code editor (client)
// for syntax highlighting, error reporting, and other code editing features.
//

#ifndef DSLSYNTAX_PARSER_H
#define DSLSYNTAX_PARSER_H

/*
 * Load the Initial Content
 * This function sets the server/parser CodeBuffer object, and parses it.
 * It frees the initial load after setting the code buffer.
 */
void parser_load_initial_content(CodeBuffer *cb, InitialLoad *initial_load);

/* 
 * Server Loop for the Parser
 * This function blocks and handles client connections via sockets.
 */
void cb_start_server(CodeBuffer *parser_cb, const char *address, int port);

/* 
 * Server Loop for the Parser
 * This function blocks and handles client connections via STDIN/STDOUT.
 */
void cb_start_stdio_server(CodeBuffer *parser_cb);

#endif //DSLSYNTAX_PARSER_H
