//
// DSL Syntax Editor
// This header file defines the DSL syntax editor, which is a part of the
// DSL (Domain Specific Language) editor library. It provides functions and
// structures for a code editor (client) to interact with a parser (server) for
// syntax highlighting, error reporting, and other code editing features.
//

#ifndef DSLSYNTAX_EDITOR_H
#define DSLSYNTAX_EDITOR_H
/* Function Prototypes - Used by Editor */

/* Function to initialise the editor side of the library */
void editor_init();

/* Function to free the editor side of the library */
void editor_free();

/* Function to create an initial load from a source string */
InitialLoad* create_initial_load(const char *unique_document_id, const char *content);

/*
 * Load the Initial Content
 * This function sets the local CodeBuffer object, after which the codeblock
 * can be used by the editor.
 * It frees the initial load after setting the code buffer.
 * It calls the communication function to send the initial load to the parser
 * The results of which will be applied to the code buffer when it arrives.
 */
void load_initial_content(CodeBuffer *cb, InitialLoad *initial_load);

// Highlights the buffer using its parse tree
void highlight_syntax(CodeBuffer *buffer);

#endif //DSLSYNTAX_EDITOR_H
