//
// DSL Syntax Common Header
// This header file defines common structures and functions used by both the
// DSL syntax parser and editor. It includes definitions for code buffers, initial loads,
// transactions, and other shared components that facilitate communication between
// the editor and parser components of the DSL editor library.
//
#ifndef TOKEN_BUFFER_H
#define TOKEN_BUFFER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h> // For uint32_t
#include <stddef.h> // For size_t

#include "utf8.h"
#include "thread_utils.h"

// Attempt to include uchar.h if available and the C standard is recent enough.
// Use __has_include if the compiler supports it (a common extension)
// or check the C standard version.
#if defined(__has_include)
#if __has_include(<uchar.h>)
#define HAVE_UCHAR_H
#include <uchar.h>
#endif
#elif __STDC_VERSION__ >= 201112L
#define HAVE_UCHAR_H
#include <uchar.h>
#endif

// Define char32_t if not provided by uchar.h
#ifndef HAVE_UCHAR_H
    // If uchar.h is not available, define char32_t using uint32_t.
    // This relies on stdint.h being available (standard since C99)
    // and char32_tbeing exactly 32 bits wide, which is standard.
    typedef uint32_t char32_t;
    // Note: U"" string literal support is a compiler feature tied to char32_t.
    // If uchar.h is missing, U"" literals might not work.
    // You would need to initialize char32_t arrays manually or use other means
    // to get UTF-32 data into your program.
    // #warning "Using uint32_t as a fallback for char32_t. U\"\" literals may not be supported."
#endif

// Simple implementation of strlen for UTF-32 (char32_t) strings
// This counts the number of char32_t units until the first zero unit.
static size_t utf32_strlen(const char32_t* str) {
    size_t length = 0;
    // The null terminator for UTF-32 is a 32-bit value of 0.
    while (*str != 0) {
        length++;
        str++;
    }
    return length;
}

// Checks against the provided list of Unicode whitespace code points.
static int utf32_isspace(char32_t c) {
    switch (c) {
        case 0x00000009: // character tabulation
        case 0x0000000A: // line feed
        case 0x0000000B: // line tabulation
        case 0x0000000C: // form feed
        case 0x0000000D: // carriage return
        case 0x00000020: // space
        case 0x00000085: // next line
        case 0x000000A0: // no-break space
        case 0x00001680: // ogham space mark
        case 0x0000180E: // mongolian vowel separator
        case 0x00002000: // en quad
        case 0x00002001: // em quad
        case 0x00002002: // en space
        case 0x00002003: // em space
        case 0x00002004: // three-per-em space
        case 0x00002005: // four-per-em space
        case 0x00002006: // six-per-em space
        case 0x00002007: // figure space
        case 0x00002008: // punctuation space
        case 0x00002009: // thin space
        case 0x0000200A: // hair space
        case 0x0000200B: // zero width space
        case 0x0000200C: // zero width non-joiner
        case 0x0000200D: // zero width joiner
        case 0x00002028: // line separator
        case 0x00002029: // paragraph separator
        case 0x0000202F: // narrow no-break space
        case 0x0000205F: // medium mathematical space
        case 0x00002060: // word joiner
        case 0x00003000: // ideographic space
        case 0x0000FEFF: // zero width non-breaking space (BOM) - often treated specially
            return 1; // It's a whitespace character from the list
        default:
            return 0; // It's not in the list
    }
}

// Simple implementation of strncpy for UTF-32 (char32_t) strings
// Copies at most n char32_t units from src to dest.
// If src is shorter than n, pads dest with null (0) char32_t units.
// If src is n units or longer, dest is NOT guaranteed to be null-terminated.
static char32_t* utf32_strncpy(char32_t* restrict dest, const char32_t* restrict src, size_t n) {
    size_t src_len = utf32_strlen(src);
    size_t copy_len = src_len;

    // If the source length is less than n, we need to copy the source
    // including the null terminator, and then pad the rest with nulls.
    if (copy_len < n) {
        // Copy the source string including the null terminator
        memcpy(dest, src, (copy_len + 1) * sizeof(char32_t));
        // Pad the rest with nulls
        memset(dest + copy_len + 1, 0, (n - (copy_len + 1)) * sizeof(char32_t));
    } else {
        // If the source is n or longer, copy exactly n units.
        // No null termination is guaranteed in the destination.
        memcpy(dest, src, n * sizeof(char32_t));
    }

    return dest;
}

/* Message Severity */
typedef enum CB_Severity {
    CB_NONE = '0',     // To ensure it is printable
    CB_INFORMATION,
    CB_WARNING,
    CB_ERROR
} CB_Severity;

/* Enumerations for CB_Token Types in the Parser Tree
 *
 * Note that tokens with a code < PARSE_TREE are leaf nodes and their position and length are the
 * mapping to the code stream, while those with codes >= PARSE_TREE are non-leaf and their position
 * overlaps with the leaf tokens */
typedef enum CB_NodeType {

    // The first real code should be >= 33 to be compatible with ASCII printable characters

    // *** Lexer Tokens - leaf nodes in the parse tree ***

    // Whitespace Tokens
    LEXER_WHITESPACE = 33,     // Represents spaces or tabs
    LEXER_EOF,                 // Represents the end of the file
    // General Tokens
    LEXER_TOKEN,               // Represents a generic token
    LEXER_UNKNOWN,             // Represents an unknown token (error)
    // Comment Tokens
    LEXER_COMMENT,             // Represents a comment
    // Literal Tokens
    LEXER_STRING_LITERAL,      // Represents string/character literals.
    LEXER_NUMBER_LITERAL,      // Represents integer or floating-point numbers.
    // Keyword
    LEXER_KEYWORD,             // A generic token representing language-specific keywords
    // Operator Tokens
    LEXER_OPERATOR,            // A generic operator token for any operator.
    LEXER_OPERATOR_ASSIGN,     // Assignment operators (`=`).
    LEXER_OPERATOR_ARITHMETIC, // Arithmetic operators (`+`, `-`, `*`, `/`).
    LEXER_OPERATOR_LOGICAL,    // Logical operators (`&&`, `||`).
    // Separator Tokens
    LEXER_SEPARATOR,           // A generic separator token for punctuation or delimiters
    LEXER_STATEMENT_SEPARATOR, // Represents the end of a statement (e.g., `;`).
    LEXER_LH_BLOCK,            // Represents a generic token for the left-hand side of a statement, code, expression, etc.
    LEXER_RH_BLOCK,            // Represents a generic token for the right-hand side of a statement, code, expression, etc.
    LEXER_LH_CODEBLOCK,        // Represents the left-hand side of a code block (e.g., `{`).
    LEXER_RH_CODEBLOCK,        // Represents the right-hand side of a code block (e.g., `}`).
    LEXER_LH_EXPR,             // Represents the left-hand side of an expression (e.g., `(`).
    LEXER_RH_EXPR,             // Represents the right-hand side of an expression (e.g., `)`).
    // Identifier Tokens
    LEXER_IDENTIFIER,          // Represents user-defined names such as variable names, function names, etc. This token can include an optional unique_i attribute to help track identifiers within the correct scope.

    // *** Parse Tree Control Tokens - non-leaf nodes in the parse tree ***

    // Start Tokens - These tokens are used to mark the beginning of a new structural element in the parse tree - Downward movement
    PARSE_TREE = 70,       // A generic structural element
                           // Specific Structure Tokens:
    PARSE_TREE_FILE,       // Represents the entire file or document.
    PARSE_TREE_CODEBLOCK,  // Represents a code block (e.g., function body, class definition, loops).
    PARSE_TREE_STATEMENT,  // Represents an individual statement (e.g., a control structure or command).
    PARSE_TREE_EXPR,       // Represents an expression (e.g., binary operations).
    PARSE_TREE_COMMENT,    // Groups multiple line comments into a comment block
    PARSE_TREE_SCOPE,      // Represents namespaces or modular scope (e.g., classes, namespaces).
    PARSE_TREE_FUNCTION,   // Represents functions or methods.
    PARSE_TREE_STRUCTURE,  // Represents higher-level structures like structs

    // *** Error and Special Tokens ***
    SYNTAX_ERROR = 120,    // Represents a generic error
    INTERNAL_ERROR         // Represents an internal parser error

    // The last code should be <= 126 to be compatible with ASCII printable characters
} CB_NodeType;

typedef enum TransactionType {
    TRANSACTION_ADDLINE = 'a', // Ensure it is printable
    TRANSACTION_DELETELINE,
    TRANSACTION_ADDCHARS,
    TRANSACTION_DELETECHARS,
    TRANSACTION_JOINLINES,
    TRANSACTION_SPLITLINE
} TransactionType;

/* Structure to represent a single parse tree node (leaf or non-leaf) */
typedef struct CB_Node {
    CB_NodeType type;         // The type attribute indicates the kind of node (e.g., keyword, identifier, operator, etc.)
    size_t pos;               // The pos attribute is used to indicate the starting position of the token in the buffer
    size_t length;            // Every token includes a length to indicate the number of characters it covers.
                              // This is especially important for maintaining line and column information in the editor
    int identifier_id;        // For LEXER_IDENTIFIER especially. The unique_id attribute is optional and can be used to uniquely identify the symbol across scopes (e.g., for renaming or searching)
    CB_Severity severity;     // The severity attribute is used to indicate the severity of the message (e.g., information, warning, error)
    char *message_code;       // The message_code attribute is used to provide a unique identifier for the message (e.g., ERR001)
    char *message;            // The message attribute is used to provide additional information or warnings associated with the node
    struct CB_Node *parent;   // The parent attribute is a pointer to the parent node in the parse tree
    struct CB_Node *child;    // The child attribute is a pointer to the first child node in the parse tree
    struct CB_Node *sibling;  // The sibling attribute is a pointer to the next sibling node in the parse tree
} CB_Node;

/* Structure to represent the ParseTree */
typedef struct CB_ParseTree {
    CB_Node *root;            // The root attribute is a pointer to the root node of the parse tree
    CB_Node *current_parent;  // The current_parent attribute is a pointer to the current_parent node in the parse tree
    CB_Node *last;            // The last attribute is a pointer to the last node added into the parse tree
} CB_ParseTree;

/* Transactions applied by the editor to the code buffer */
typedef struct Transaction {
    TransactionType type;
    int pos_line;
    int pos_col;
    char *content; /* For ADDLINE, ADDCHARS, etc. */
    int count;         /* For DELETECHARS */
} Transaction;

/* Communication Function Pointer Structure */
typedef struct CommunicationFunctions CommunicationFunctions;

/* typedef for the parser function void parse(CodeBuffer *cb); */
struct CodeBuffer;
typedef void (*ParserFunction)(struct CodeBuffer *cb);

/* Structure for the attributes of a character in the code buffer.
 * These are designed to assist an editor in providing fast access for
 * syntax highlighting and error messages. An editor can then access
 * the parser node for full details if need be */
typedef struct CodeBufferCharacter {
    char32_t character[4];       // The character itself, stored as upto 4 UTF-32 codepoints
    char32_t *heap_character;    // Pointer to the heap-allocated character, if needed
    size_t codepoints;           // Number of codepoints in the character
    char token_type;             // CB_NodeType coded as char - used to determine highlighting
    char severity;               // CB_Severity coded as char - indicates is a message is associated with the character
    char subtree_type;           // CB_NodeType coded as char - indicates if the character starts a parse tree, an editor might use this for code folding
    unsigned char subtree_lines; // Number of lines in the subtree - used for code folding
                                 // This is zero-based, so 0 means 1 line.
                                 // 255 means 256 lines or more.
                                 // An editor is expected to use this to indicate folding options visually
                                 // but will likely need to use the parse tree to get the full details
    CB_Node *node;               // Pointer to the parse tree node for this character
                                 // This is used for fast access to the parse tree node for syntax highlighting and error messages
                                 // Editors should not store this pointer as it may change when the parse tree is updated
} CodeBufferCharacter;

/* Structure for a line of characters (null terminated) with a line length */
typedef struct CodeBufferLine {
    CodeBufferCharacter *characters; // Array of characters in the line
                                     // Each line is a null-terminated array of CodeBufferCharacter.
                                     // This allows for fast access to the characters in the code buffer.
                                     // The first character in each line is the first character of the line.
                                     // The last character in each line is the newline character (if present).
                                     // Lines are not guaranteed to be null-terminated, but they are expected to be.
                                     // Character boundaries (e.g. Unicode glyphs) are determined by the editor and
                                     // parser
    size_t length;                   // Length of the line (excluding the null terminator)
} CodeBufferLine;

/* Structure of the main shared code buffer, synced between editor and parser */
typedef struct CodeBuffer {
    // Header
    char *unique_document_id;
    size_t change_version;           // Version number of the document

    // Contents
    size_t line_count;               // The number of lines in the code buffer
    CodeBufferLine *lines;           // Array of lines

    // Parse Result - most recent version
    CB_ParseTree *parse_tree;        // Pointer to the parse tree
    CB_Severity highest_severity;

    // Snapshot information
    int snapshot_number;             // The change version of the snapshot
    size_t snapshot_line_count;      // The line count at the last snapshot
    CodeBufferLine *snapshot_lines;  // The lines at the last snapshot

    Transaction *transactions;       // Transactions applied after the last snapshot
    size_t transaction_count;

    // Communication Functions - injected by the editor or parser
    CommunicationFunctions *communication_functions;
    ParserFunction parser_function;  // Function to parse the code buffer
} CodeBuffer;

/* Messages sent between the parser and editor */

/* Full Contents of the code - for the initial load (or re-sync) */
typedef struct InitialLoad {
    char *unique_document_id;
    size_t change_version;
    CodeBufferLine *lines;
    size_t line_count;
} InitialLoad;

/* Transactions applied by the editor to the code buffer since the last parse */
typedef struct Delta {
    size_t change_version;
    Transaction *transactions;
    size_t transaction_count;
} Delta;

/* Result of parsing a document */
typedef struct ParseResult {
    char *unique_document_id;
    size_t change_version;
    CB_Node *parse_tree;    /* Pointer to the parse tree */
} ParseResult;

/*
 * Communication functions - this has a number of typedefs for the functions
 * that the parser and editor need to call to communicate with each other,
 * and a structure of pointers of these functions. The structure is passed to
 * the parser and editor to allow them to call the functions. Different
 * communication methods can be implemented by changing the functions in the
 * structure.
 * */

/* Function to send an initial load to the parser */
typedef CB_ParseTree* (*SendInitialLoad)(CommunicationFunctions *comm_block, InitialLoad *initial_load);

/* Function to send a delta to the parser */
typedef CB_ParseTree* (*SendDelta)(CommunicationFunctions *comm_block, Delta *delta);

/* Communication Function Pointer Structure */
struct CommunicationFunctions {
    SendInitialLoad send_initial_load;
    SendDelta send_delta;
    void* comms_data; // Pointer to data for the communication functions
};

/* Library sync functions - these are called by the communication functions */
/* Function to apply a delta to the code buffer */
void base_replay_delta(CodeBuffer *cb, Delta *delta);

/* Function to apply the initial load to the code buffer */
/* The initial load is freed after applying */
// Depricate void apply_initial_load(CodeBuffer *cb, InitialLoad *initial_load);

/* Function to apply the ParseResult to the code buffer */
void apply_parse_result(CodeBuffer *cb, ParseResult *parse_result);

/* Function to get a delta from the code buffer */
Delta* get_delta(CodeBuffer *cb);



/* Inproc communications functions factory */
CommunicationFunctions* create_inproc_communication_functions(CodeBuffer *parser_cb);

// Free the inproc communication functions
void free_inproc_communication_functions(CommunicationFunctions *comm);

/* Function Prototypes - Utility / Common Functions */

/* Common utility to safely reallocate a buffer */
void* safe_realloc(void *ptr, size_t size);

/*
 * Converts a single UTF-32 codepoint to its UTF-8 representation.
 *
 * Parameters:
 * utf32_codepoint - The unsigned int representing the UTF-32 codepoint.
 * utf8_buffer     - A pointer to the char array where the UTF-8 sequence will be written.
 * buffer_size     - The maximum size of the utf8_buffer.
 *
 * Returns:
 * The number of bytes written to utf8_buffer (1 to 4).
 * Returns 0 if the buffer is too small for the resulting UTF-8 sequence.
 * Returns 1 and writes '?' if the codepoint is invalid.
 */
size_t utf32_to_utf8_char(unsigned int utf32_codepoint, char *utf8_buffer, size_t buffer_size);

/* Utility to get the utf8 length of a utf32 string */
size_t utf32_utf8_length(const char32_t* utf32, size_t length);

/* Utility to convert an utf32 string to utf8 */
char* utf32_to_utf8(const char32_t*utf32, size_t length);

/* Utility to convert an utf32 string to ascii (invalid characters are replaced with '?') */
char* utf32_to_ascii(const char32_t*utf32, size_t length);

/* Utility to convert a null terminated utf8 or ascii string to utf32
 * `length` is set to the length of the returned utf32 string */
char32_t* utf8_to_utf32(const char *utf8, size_t *length);

/* Function to create a new CodeBuffer */
CodeBuffer* create_code_buffer(CommunicationFunctions *comm, ParserFunction parser_function);

/* Applying Transactions */
void editor_apply_transaction(CodeBuffer *cb, Transaction transaction);

/* Function to take a snapshot of the buffer */
void snapshot(CodeBuffer *cb);

/* Function to copy the snapshot to the codebuffer */
void copy_snapshot_to_codebuffer(CodeBuffer *cb);

/* Snapshotting and Getting Deltas */
Delta* snapshot_and_get_delta(CodeBuffer *cb);

/* Utility Functions */
void free_delta(Delta *delta);
void print_code_buffer(CodeBuffer *cb);
void free_code_buffer(CodeBuffer *cb);

// Function to return part of the code buffer based on the buffer position and length.
// It also sets the `line` and `col` position of the buffer (if they are not null, in which case they are ignored).
// If `value` is not NULL, it will set the value to the part of the buffer (caller must free() it) in utf8.
// Returns NULL if the buffer position is out of bounds, otherwise it returns a pointer into the buffer (utf32), not malloc'd
// and as part of the CodeBufferLine structure ends at the end of the line.
// Note this function and the library in general exclude newline characters from the buffer, meaning
// they are not included in the length of the buffer or buffer position calculations.
// If the col and length are greater than the line length, it will return the line from the col position to the end of the line.
// In general, it is preferred that parsers split tokens on newline characters so that tokens don't span multiple lines.
CodeBufferCharacter* get_code_buffer_part(CodeBuffer *cb, size_t pos, size_t length, size_t *line, size_t *col, char** value);

// Get code buffer total length (including a newline (0x0A) between each line)
size_t get_code_buffer_length(CodeBuffer *cb);

/* FUNCTIONS FOR THE PARSER TO USE */

CB_ParseTree* cb_create_token_buffer();

// Function to free the CB_ParseTree and its nodes
void cb_free_token_buffer(CB_ParseTree *root);

// Function to extract the source code from the CodeBuffer in UTF-8 format
char* get_code_buffer_source(CodeBuffer *cb);

/* Basic CB_Token Management */

/* Create a blank CB_Node */
CB_Node cb_create_node(CB_NodeType type, size_t pos, size_t length);

/* Set the current_parent node to the 'parent' node */
void cb_set_current_parent_to_node(CB_ParseTree *tb, CB_Node *parent);

/* Set the current_parent node to the root node */
void cb_set_current_parent_to_root_node(CB_ParseTree *tb);

/* Set the current_parent node to the last node */
void cb_set_current_parent_to_last_node(CB_ParseTree *tb);

/* Set the current_parent node to the parent node */
void cb_set_current_parent_to_grandparent(CB_ParseTree *tb);

/* Set the current_parent node to the last child node */
void cb_set_current_parent_to_last_child(CB_ParseTree *tb);

/* Add a new node to the parse tree as the last child to the current_parent position */
void cb_add_child_node(CB_ParseTree *tb, CB_Node node);

/* Add a new node to the parse tree as the first child of the current_parent node */
void cb_add_first_child_node(CB_ParseTree *tb, CB_Node node);

/* Add a new node to the parse tree as sibling to the current_parent position */
void cb_add_sibling_node(CB_ParseTree *tb, CB_Node node);

/* Tree Walking */

/* typedef for the callback function to walk the tree */
typedef void (*CB_WalkTreeCallback)(CB_Node *node, size_t depth, void *user_data);

/* top-down tree walk */
void cb_walk_tree_top_down(CB_ParseTree *tb, CB_WalkTreeCallback callback, void *user_data);

/* bottom-up tree walk */
void cb_walk_tree_bottom_up(CB_ParseTree *tb, CB_WalkTreeCallback callback, void *user_data);

/*
 * Functions to support intelligently creating the CB_ParseTree for clients that are creating tokens
 * from an Abstract Syntax Tree (AST).
 *
 * In general, users will need to walk the AST and append tokens to the CB_ParseTree. However, because
 * the CB_ParseTree is both a Parse Tree and an ordered list of Lexer tokens, the added tokens must be ordered
 * as per the source, which may be different to the order from the AST walker. Also, there can be no missing tokens
 * in the CB_ParseTree to ensure column and line numbers are correct. And finally, TOKENS might be moved between
 * subtrees, whitespace tokens might be moved outside the tree, for example, and a statement terminator might
 * be moved into the end of a statement tree.
 *
 * This may be problematic for users because the AST will likely have had unnecessary token removed
 * (like ";" or whitespaces).
 *
 * The following functions help to manage this complexity by providing a way to add tokens to the CB_ParseTree
 */

/* Function to ensure the CB_ParseTree tree is ordered correctly */
void cb_order_tree(CB_ParseTree *tb);

/*
 * Typedef for the callback function to generate/lookup missing tokens based on position and length.
 * The token_chars points to the code buffer at the position and length.
 * This function should return a CB_Node for the given position and length.
 * The length of the returned token should be the same or less than the requested length.
 *
 * Note that for performance reasons 'token_chars' is a pointer in to the line buffer. This means it
 * only contains characters up to the character before the newline. The callback function can access
 * its own private buffer (or lexer tokens buffer) accessed via 'user_data' to get the full token.
 *
 * Alternatively, the callback function should only use the characters up to the length by returning a shorter
 * token if necessary. In this case the token length should be one more than the length of the token_chars
 * to include the newline character.
 *
 * THIS FUNCTION MUST RETURN A TOKEN OF LENGTH 1 OR MORE - OTHERWISE PANIC WITH EXIT 1
 */
typedef CB_Node (*CB_GetTokenCallback)(void *user_data, size_t pos, size_t length, CodeBufferCharacter* token_chars);

/* Default callback function to generate/lookup missing tokens based on position and length            */
/* This function just returns a LEXER_WHITESPACE, LEXER_EOF, or LEXER_COMMENT tokens                   */
/* This would be OK for basic implementations where the lexer might have skipped non-essential tokens  */
/* This function can be used as is - or could be called by a user-defined callback function            */
/*                                                                                                     */
/* For a better implementation, users should provide their own callback function                       */
CB_Node cb_default_get_token_callback(void *user_data, size_t pos, size_t length, CodeBufferCharacter* token_chars);

/*
 * Function to add missing tokens to the CB_ParseTree using a callback function to access or perhaps generate
 * the missing tokens. It calls the callback function with the user_data and the ordinal of missing tokens.
 *
 * This optional function should be called after all tokens have been added to the CB_ParseTree
 */
void cb_add_missing_tokens(CB_ParseTree *tb, CodeBuffer *cb, CB_GetTokenCallback callback, void *user_data);

/*
 * Function to tweak the position of a token in the CB_ParseTree. This is useful for improving the position of
 * tokens for syntax highlighting. The way the parse tree is built may not always result in the best position for
 * tokens, the following tweaks are implemented:
 * - Statement terminators are moved to the end of the statement's tree
 * - Open and close brackets are moved to the beginning and end of the tree
 * moving tokens up or down the tree
 * - Leading and Training whitespace tokens moved outside the tree,
 * - Terminators moved to the beginning or end of trees using a heuristic based on if
 *   there are already terminators in the tree.
 *
 * This optional function should be called after all tokens have been added to the CB_ParseTree
 * and cb_add_missing_tokens() has been called (if necessary).
 */
void cb_tweak_tree_positions(CB_ParseTree *tb);

/* Utility Functions */
/* Function to print the CB_ParseTree for debugging */
void cb_print_token_buffer(CodeBuffer *cb, CB_ParseTree *tb);

/* Function to validate a parse tree - are nodes on order, and are there no gaps,
 * are pointers correct, etc. */
void cb_validate_tree(CB_ParseTree *tb);

/* Function to convert CB_NodeType to a string */
const char* cb_token_type_to_string(CB_NodeType type);


/*TODO - This is where I am adding functions i have moved to common */

/* Utility to convert transaction code to text */
const char* transaction_type_to_string(TransactionType type);

/*
 * Base functionality to Load the Initial Content
 * This function sets the local CodeBuffer object, after which the codeblock
 * can be used.
 * It frees the initial load after setting the code buffer.
 */
void base_load_initial_content(CodeBuffer *cb, InitialLoad *initial_load);

/*
 * Base functionality to parse the buffer and create the parse tree.
 */
void base_parse_buffer(CodeBuffer *cb);

/* Function to apply a single transaction - this is the internal base functionality for applying and re-applying transactions*/
void base_apply_transaction(CodeBuffer *cb, Transaction transaction);

#endif /* TOKEN_BUFFER_H */
