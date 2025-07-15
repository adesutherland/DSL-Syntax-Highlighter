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

#endif //DSLSYNTAX_PARSER_H
