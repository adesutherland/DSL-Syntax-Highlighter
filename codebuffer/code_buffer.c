/* code_buffer.c */

#include <unistd.h>

#include "dslsyntax_common.h"


/* Common / Utility Functions */

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
size_t utf32_to_utf8_char(unsigned int utf32_codepoint, char *utf8_buffer, size_t buffer_size) {
    if (utf8_buffer == NULL) {
        return 0; /* Or handle error appropriately */
    }

    if ((utf32_codepoint >= 0xD800 && utf32_codepoint <= 0xDFFF) || utf32_codepoint > 0x10FFFF) {
        if (buffer_size >= 1) {
            utf8_buffer[0] = '?'; /* Invalid code point mapped to '?' */
            return 1;
        } else {
            return 0; /* Buffer too small */
        }
    } else if (utf32_codepoint < 0x80) {
        /* 1-byte sequence (0xxxxxxx) */
        if (buffer_size >= 1) {
            utf8_buffer[0] = (char)utf32_codepoint;
            return 1;
        } else {
            return 0; /* Buffer too small */
        }
    } else if (utf32_codepoint < 0x800) {
        /* 2-byte sequence (110xxxxx 10xxxxxx) */
        if (buffer_size >= 2) {
            utf8_buffer[0] = (char)(0xC0 | (utf32_codepoint >> 6));
            utf8_buffer[1] = (char)(0x80 | (utf32_codepoint & 0x3F));
            return 2;
        } else {
            return 0; /* Buffer too small */
        }
    } else if (utf32_codepoint < 0x10000) {
        /* 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx) */
        if (buffer_size >= 3) {
            utf8_buffer[0] = (char)(0xE0 | (utf32_codepoint >> 12));
            utf8_buffer[1] = (char)(0x80 | ((utf32_codepoint >> 6) & 0x3F));
            utf8_buffer[2] = (char)(0x80 | (utf32_codepoint & 0x3F));
            return 3;
        } else {
            return 0; /* Buffer too small */
        }
    } else { /* utf32_codepoint <= 0x10FFFF */
        /* 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx) */
        if (buffer_size >= 4) {
            utf8_buffer[0] = (char)(0xF0 | (utf32_codepoint >> 18));
            utf8_buffer[1] = (char)(0x80 | ((utf32_codepoint >> 12) & 0x3F));
            utf8_buffer[2] = (char)(0x80 | ((utf32_codepoint >> 6) & 0x3F));
            utf8_buffer[3] = (char)(0x80 | (utf32_codepoint & 0x3F));
            return 4;
        } else {
            return 0; /* Buffer too small */
        }
    }
}

/* Utility to get the utf8 length of a utf32 string */
size_t utf32_utf8_length(const char32_t* utf32, size_t length) {
    size_t utf8_length = 0;
    for (size_t i = 0; i < length; i++) {
        if ((utf32[i] >= 0xD800 && utf32[i] <= 0xDFFF) || utf32[i] > 0x10FFFF) {
            utf8_length += 1; // Invalid code point mapped to '?' (1 byte)
        } else if (utf32[i] < 0x80) {
            utf8_length++;
        } else if (utf32[i] < 0x800) {
            utf8_length += 2;
        } else if (utf32[i] < 0x10000) {
            utf8_length += 3;
        } else {
            utf8_length += 4;
        }
    }
    return utf8_length;
}

/* Utility to convert an utf32 string to utf8 */
char* utf32_to_utf8(const char32_t*utf32, size_t length) {
    size_t utf8_length = 0;
    for (size_t i = 0; i < length; i++) {
        if ((utf32[i] >= 0xD800 && utf32[i] <= 0xDFFF) || utf32[i] > 0x10FFFF) {
            utf8_length += 1; // Invalid code point mapped to '?' (1 byte)
        } else if (utf32[i] < 0x80) {
            utf8_length++;
        } else if (utf32[i] < 0x800) {
            utf8_length += 2;
        } else if (utf32[i] < 0x10000) {
            utf8_length += 3;
        } else {
            utf8_length += 4;
        }
    }

    char *utf8 = (char *)malloc(utf8_length + 1);
    if (!utf8) {
        perror("Failed to allocate memory for utf8 string");
        exit(EXIT_FAILURE);
    }

    size_t pos = 0;
    for (size_t i = 0; i < length; i++) {
        if ((utf32[i] >= 0xD800 && utf32[i] <= 0xDFFF) || utf32[i] > 0x10FFFF) {
            utf8[pos++] = '?'; // Invalid code point mapped to '?' (1 byte)
        } else if (utf32[i] < 0x80) {
            utf8[pos++] = (char)utf32[i];
        } else if (utf32[i] < 0x800) {
            utf8[pos++] = (char)(0xC0 | (utf32[i] >> 6));
            utf8[pos++] = (char)(0x80 | (utf32[i] & 0x3F));
        } else if (utf32[i] < 0x10000) {
            utf8[pos++] = (char)(0xE0 | (utf32[i] >> 12));
            utf8[pos++] = (char)(0x80 | ((utf32[i] >> 6) & 0x3F));
            utf8[pos++] = (char)(0x80 | (utf32[i] & 0x3F));
        } else {
            utf8[pos++] = (char)(0xF0 | (utf32[i] >> 18));
            utf8[pos++] = (char)(0x80 | ((utf32[i] >> 12) & 0x3F));
            utf8[pos++] = (char)(0x80 | ((utf32[i] >> 6) & 0x3F));
            utf8[pos++] = (char)(0x80 | (utf32[i] & 0x3F));
        }
    }
    utf8[pos] = '\0';

    return utf8;
}

/* Utility to convert an utf32 string to ascii (invalid characters are replaced with '?') */
char* utf32_to_ascii(const char32_t*utf32, size_t length) {
    char *ascii = (char *)malloc(length + 1);
    if (!ascii) {
        perror("Failed to allocate memory for ascii string");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < length; i++) {
        if (utf32[i] < 0x80) {
            ascii[i] = (char)utf32[i];
        } else {
            ascii[i] = '?';
        }
    }
    ascii[length] = '\0';

    return ascii;
}

/* Utility to convert a null terminated utf8 or ascii string to utf32 */
char32_t* utf8_to_utf32(const char* utf8_string, size_t *length) {
    if (!utf8_string) {
        return NULL; // Handle NULL input
    }

    // First pass: Calculate the number of UTF-32 code points.
    size_t utf32_length = 0;
    const char* temp_utf8_ptr = utf8_string;
    utf8_int32_t  decoded_code_point; // Variable to hold the decoded code point

    while (*temp_utf8_ptr) {
        // utf8codepoint() decodes the next code point and advances the pointer.
        temp_utf8_ptr = utf8codepoint(temp_utf8_ptr, &decoded_code_point);
        (void)decoded_code_point; // Suppress unused variable warning if needed
        utf32_length++;
    }

    // Allocate memory for the UTF-32 string plus null terminator.
    // Each char32_t is typically 4 bytes.
    char32_t* utf32_string = (char32_t*)malloc((utf32_length + 1) * sizeof(char32_t));
    if (!utf32_string) {
        perror("Failed to allocate memory for utf32 string");
        return NULL; // Indicate allocation failure
    }

    // Second pass: Decode and copy the UTF-32 code points.
    size_t utf32_pos = 0;
    temp_utf8_ptr = utf8_string; // Reset pointer to the beginning

    while (*temp_utf8_ptr) {
        // Decode the next code point and store it.
        temp_utf8_ptr = utf8codepoint(temp_utf8_ptr, &decoded_code_point);
        utf32_string[utf32_pos++] = decoded_code_point;
    }

    // Null-terminate the UTF-32 string.
    utf32_string[utf32_pos] = 0;

    if (length) *length = utf32_length;

    return utf32_string; // Return the newly allocated UTF-32 string
}

/* Utility to convert the first line of a null terminated utf8 or ascii string to utf32 */
/* Newline is not included in the output */
char32_t* first_line_utf8_to_utf32(const char* utf8_string, size_t *length) {
    if (!utf8_string) {
        return NULL; // Handle NULL input
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
    // Each char32_t is typically 4 bytes.
    char32_t* utf32_string = (char32_t*)malloc((utf32_length) * sizeof(char32_t));
    if (!utf32_string) {
        perror("Failed to allocate memory for utf32 string");
        return NULL; // Indicate allocation failure
    }

    // Second pass: Decode and copy the UTF-32 code points.
    size_t utf32_pos = 0;
    temp_utf8_ptr = utf8_string; // Reset pointer to the beginning

    while (*temp_utf8_ptr && *temp_utf8_ptr != '\n') {
        // Decode the next code point and store it.
        temp_utf8_ptr = utf8codepoint(temp_utf8_ptr, &decoded_code_point);
        utf32_string[utf32_pos++] = decoded_code_point;
    }

    if (length) *length = utf32_length;

    return utf32_string; // Return the newly allocated UTF-32 string
}

/* Function to apply a single transaction - this is the internal base functionality for applying and re-applying transactions*/
static void base_apply_transaction(CodeBuffer *cb, Transaction transaction);

/* Common utility to safely reallocate a buffer */
void* safe_realloc(void *ptr, size_t size) {
    void *new_ptr;
    if (ptr) new_ptr = realloc(ptr, size);
    else {
        // If ptr is NULL, we need to allocate new memory and set values to zero
        new_ptr = malloc(size);
        if (new_ptr) {
            memset(new_ptr, 0, size); // Initialize the allocated memory to zero
        }
    }
    if (!new_ptr) {
        perror("PANIC: Failed to reallocate memory");
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}


/* Function to create a new CodeBuffer */
CodeBuffer* create_code_buffer(CommunicationFunctions *comm, ParserFunction parser_function) {
    CodeBuffer *cb = (CodeBuffer *)malloc(sizeof(CodeBuffer));
    if (!cb) {
        perror("Failed to allocate memory for CodeBuffer");
        exit(EXIT_FAILURE);
    }

    cb->communication_functions = comm;
    cb->parser_function = parser_function;
    cb->unique_document_id = NULL;
    cb->snapshot_number = 0;
    cb->lines = NULL;
    cb->line_count = 0;
    cb->dirty_lines = NULL;
    cb->parse_tree = NULL;
    cb->node_lines = NULL;
    cb->highest_severity = CB_NONE;
    cb->transactions = NULL;
    cb->transaction_count = 0;
    cb->change_version = 0;

    return cb;
}

/* Editor Functions */



/* Function to apply the initial load to the code buffer - editor side */
/* The initial load is freed after applying */
/* Depricate
void apply_initial_load(CodeBuffer *cb, InitialLoad *initial_load) {
    cb->unique_document_id = initial_load->unique_document_id;
    initial_load->unique_document_id = 0;

    cb->lines = initial_load->lines;
    initial_load->lines = NULL;

    cb->line_lengths = initial_load->line_lengths;
    initial_load->line_lengths = 0;

    cb->line_count = initial_load->line_count;
    initial_load->line_count = 0;

    cb->change_version = initial_load->change_version;
    initial_load->change_version = 0;
}
*/

/* Applying Transactions */

/* Function to apply a single transaction - this is the internal base functionality for applying and re-applying transactions*/
static void base_apply_transaction(CodeBuffer *cb, Transaction transaction) {
    if (!cb) return;
    char32_t*new_line;
    char32_t*utf32_content;
    size_t content_len;
    int has_attributes = 0;
    if (cb->attributes) {
        has_attributes = 1; // If attributes are set, we need to handle them and node_lines
    }
    switch (transaction.type) {
        case TRANSACTION_ADDLINE:
            if (transaction.pos_line > cb->line_count) {
                fprintf(stderr, "AddLine Error: Line number out of bounds\n");
                return;
            }
            // Lines
            cb->lines = (char32_t**)safe_realloc(cb->lines, sizeof(char32_t*) * (cb->line_count + 1)); // NOLINT(*-suspicious-realloc-usage)
            // Line lengths
            cb->line_lengths = (size_t*)safe_realloc(cb->line_lengths, sizeof(size_t) * (cb->line_count + 1));
            // attributes
            if (has_attributes) cb->attributes = (CodeBufferCharAttributes**)safe_realloc(cb->attributes, sizeof(CodeBufferCharAttributes*) * (cb->line_count + 1));
            // node_lines
            if (has_attributes) cb->node_lines = (CB_Node***)safe_realloc(cb->node_lines, sizeof(CB_Node**) * (cb->line_count + 1));
            // dirty_lines
            cb->dirty_lines = (char*)safe_realloc(cb->dirty_lines, sizeof(char) * (cb->line_count + 1));
            /* Shift lines down */
            for (size_t i = cb->line_count; i > (size_t)transaction.pos_line; i--) {
                cb->lines[i] = cb->lines[i - 1];
                cb->line_lengths[i] = cb->line_lengths[i - 1];
                if (has_attributes) cb->attributes[i] = cb->attributes[i - 1];
                if (has_attributes) cb->node_lines[i] = cb->node_lines[i - 1];
                cb->dirty_lines[i] = cb->dirty_lines[i - 1];
            }
            new_line = utf8_to_utf32(transaction.content, &(cb->line_lengths[transaction.pos_line]));
            cb->lines[transaction.pos_line] = new_line;
            if (!cb->lines[transaction.pos_line]) {
                perror("Failed to allocate memory for new line");
                exit(EXIT_FAILURE);
            }
            // attributes
            if (has_attributes) {
                cb->attributes[transaction.pos_line] = (CodeBufferCharAttributes*)malloc(sizeof(CodeBufferCharAttributes) * cb->line_lengths[transaction.pos_line]);
                if (!cb->attributes[transaction.pos_line]) {
                    perror("Failed to allocate memory for attributes");
                    exit(EXIT_FAILURE);
                }
                for (size_t i = 0; i < cb->line_lengths[transaction.pos_line]; i++) {
                    cb->attributes[transaction.pos_line][i].token_type = LEXER_TOKEN;
                    cb->attributes[transaction.pos_line][i].severity = CB_NONE;
                    cb->attributes[transaction.pos_line][i].subtree_type = LEXER_TOKEN;
                    cb->attributes[transaction.pos_line][i].subtree_lines = 0;
                }
                // node_lines
                cb->node_lines[transaction.pos_line] = (CB_Node**)malloc(sizeof(CB_Node*) * (cb->line_lengths[transaction.pos_line] + 1));
                if (!cb->node_lines[transaction.pos_line]) {
                    perror("Failed to allocate memory for node_lines");
                    exit(EXIT_FAILURE);
                }
                for (size_t i = 0; i < cb->line_lengths[transaction.pos_line]; i++) {
                    cb->node_lines[transaction.pos_line][i] = NULL;
                }
                cb->node_lines[transaction.pos_line][cb->line_lengths[transaction.pos_line]] = NULL; // Null-terminate the line
            }
            // dirty_lines
            cb->dirty_lines[transaction.pos_line] = 1; // Dirty after adding a line
            cb->line_count++;
            break;

        case TRANSACTION_DELETELINE:
            if (transaction.pos_line >= cb->line_count) {
                fprintf(stderr, "DeleteLine Error: Line number out of bounds\n");
                return;
            }
            free(cb->lines[transaction.pos_line]);
            /* Shift lines up */
            for (size_t i = transaction.pos_line; i < cb->line_count - 1; i++) {
                cb->lines[i] = cb->lines[i + 1];
            }
            cb->lines = (char32_t**)safe_realloc(cb->lines, sizeof(char32_t*) * cb->line_count);
            // Shift line lengths up
            for (size_t i = transaction.pos_line; i < cb->line_count - 1; i++) {
                cb->line_lengths[i] = cb->line_lengths[i + 1];
            }
            cb->line_lengths = (size_t*)safe_realloc(cb->line_lengths, sizeof(size_t) * cb->line_count);
            // Shift attributes up
            if (has_attributes) {
                for (size_t i = transaction.pos_line; i < cb->line_count - 1; i++) {
                    cb->attributes[i] = cb->attributes[i + 1];
                }
                cb->attributes = (CodeBufferCharAttributes**)safe_realloc(cb->attributes, sizeof(CodeBufferCharAttributes*) * cb->line_count);
                // Shift node_lines up
                for (size_t i = transaction.pos_line; i < cb->line_count - 1; i++) {
                    cb->node_lines[i] = cb->node_lines[i + 1];
                }
                cb->node_lines = (CB_Node***)safe_realloc(cb->node_lines, sizeof(CB_Node**) * cb->line_count);
            }
            // Shift dirty_lines up
            for (size_t i = transaction.pos_line; i < cb->line_count - 1; i++) {
                cb->dirty_lines[i] = cb->dirty_lines[i + 1];
            }
            cb->dirty_lines = (char*)safe_realloc(cb->dirty_lines, sizeof(char) * cb->line_count);
            // Update the line count
            cb->line_count--;
            break;

        case TRANSACTION_ADDCHARS:
            if (transaction.pos_line >= cb->line_count) {
                fprintf(stderr, "AddChars Error: Line number out of bounds\n");
                return;
            }
            if (transaction.pos_col > cb->line_lengths[transaction.pos_line]) {
                fprintf(stderr, "AddChars Error: Column number out of bounds\n");
                return;
            }
            {
                size_t len = cb->line_lengths[transaction.pos_line];
                utf32_content = utf8_to_utf32(transaction.content, &content_len);
                size_t new_len = len + content_len;
                // Update the line
                new_line = (char32_t*)safe_realloc(cb->lines[transaction.pos_line], new_len * sizeof(char32_t));
                cb->lines[transaction.pos_line] = new_line;
                // Move the existing content after the position
                memmove(cb->lines[transaction.pos_line] + transaction.pos_col + content_len, cb->lines[transaction.pos_line] + transaction.pos_col,
                        (len - transaction.pos_col) * sizeof(char32_t));
                // insert the new content at the position
                memcpy(cb->lines[transaction.pos_line] + transaction.pos_col, utf32_content, content_len * sizeof(char32_t));
                // Free the utf32_content
                free(utf32_content);
                if (has_attributes) {
                    // Update attributes - this should apply the same attributes as the line and position being added to
                    cb->attributes[transaction.pos_line] = (CodeBufferCharAttributes*)safe_realloc(cb->attributes[transaction.pos_line],
                                                                                                   sizeof(CodeBufferCharAttributes) * new_len);
                    // Move the existing attributes after the position
                    memmove(cb->attributes[transaction.pos_line] + transaction.pos_col + content_len,
                            cb->attributes[transaction.pos_line] + transaction.pos_col,
                            (len - transaction.pos_col) * sizeof(CodeBufferCharAttributes));
                    // Insert the new attributes at the position
                    for (size_t i = len; i < new_len; i++) {
                        cb->attributes[transaction.pos_line][i].token_type = cb->attributes[transaction.pos_line][transaction.pos_col].token_type;
                        cb->attributes[transaction.pos_line][i].severity = cb->attributes[transaction.pos_line][transaction.pos_col].severity;
                        cb->attributes[transaction.pos_line][i].subtree_type = cb->attributes[transaction.pos_line][transaction.pos_col].subtree_type;
                        cb->attributes[transaction.pos_line][i].subtree_lines = cb->attributes[transaction.pos_line][transaction.pos_col].subtree_lines;
                    }

                    // Update the node_lines - this should apply the same node as the line and position being added to
                    cb->node_lines[transaction.pos_line] = (CB_Node**)safe_realloc(cb->node_lines[transaction.pos_line],
                                                                                   sizeof(CB_Node*) * new_len);
                    // Move the existing node_lines after the position
                    memmove(cb->node_lines[transaction.pos_line] + transaction.pos_col + content_len,
                            cb->node_lines[transaction.pos_line] + transaction.pos_col,
                            (len - transaction.pos_col) * sizeof(CB_Node*));
                    // Insert the new node_lines at the position
                    for (size_t i = len; i < new_len; i++) {
                        cb->node_lines[transaction.pos_line][i] = cb->node_lines[transaction.pos_line][transaction.pos_col];
                    }
                }
                // Update the dirty line
                cb->dirty_lines[transaction.pos_line] = 1; // Mark the line as dirty
                // Update the line length
                cb->line_lengths[transaction.pos_line] = new_len;
            }
            break;

        case TRANSACTION_DELETECHARS:
            if (transaction.pos_line >= cb->line_count) {
                fprintf(stderr, "DeleteChars Error: Line number out of bounds\n");
                return;
            }
            if (transaction.pos_col + transaction.count > cb->line_lengths[transaction.pos_line]) {
                fprintf(stderr, "DeleteChars Error: Column and count out of bounds\n");
                return;
            }
            {
                char32_t* line = cb->lines[transaction.pos_line];
                size_t len = cb->line_lengths[transaction.pos_line];
                size_t new_len = len - transaction.count;
                // Move the existing content after the position
                if (transaction.pos_col + transaction.count < len) {
                    memmove(line + transaction.pos_col, line + transaction.pos_col + transaction.count,
                            (len - transaction.pos_col - transaction.count) * sizeof(char32_t));
                }
                // Shrink the line
                new_line = (char32_t*)safe_realloc(line, new_len * sizeof(char32_t));
                cb->lines[transaction.pos_line] = new_line;
                if (has_attributes) {
                    // Update attributes
                    // Move the existing attributes after the position
                    memmove(cb->attributes[transaction.pos_line] + transaction.pos_col,
                            cb->attributes[transaction.pos_line] + transaction.pos_col + transaction.count,
                            (len - transaction.pos_col - transaction.count) * sizeof(CodeBufferCharAttributes));
                    // Shrink the attributes
                    cb->attributes[transaction.pos_line] = (CodeBufferCharAttributes*)safe_realloc(cb->attributes[transaction.pos_line],
                                                                                                   sizeof(CodeBufferCharAttributes) * new_len);
                    // Update node_lines
                    // Move the existing node_lines after the position
                    memmove(cb->node_lines[transaction.pos_line] + transaction.pos_col,
                            cb->node_lines[transaction.pos_line] + transaction.pos_col + transaction.count,
                            (len - transaction.pos_col - transaction.count) * sizeof(CB_Node*));
                    // Shrink the node_lines
                    cb->node_lines[transaction.pos_line] = (CB_Node**)safe_realloc(cb->node_lines[transaction.pos_line],
                                                                                   sizeof(CB_Node*) * new_len);
                }
                // Update the dirty line
                cb->dirty_lines[transaction.pos_line] = 1; // Mark the line as dirty
                // Update the line length
                cb->line_lengths[transaction.pos_line] = new_len;
            }
            break;

        case TRANSACTION_JOINLINES:
            if (transaction.pos_line >= cb->line_count - 1) {
                fprintf(stderr, "JoinLines Error: Line number out of bounds\n");
                return;
            }
            {
                size_t new_len = cb->line_lengths[transaction.pos_line] + cb->line_lengths[transaction.pos_line + 1];
                char32_t*joined_line = (char32_t*)malloc(new_len * sizeof(char32_t));
                if (!joined_line) {
                    perror("Failed to allocate memory for JoinLines");
                    exit(EXIT_FAILURE);
                }
                // Mark the line as dirty
                cb->dirty_lines[transaction.pos_line] = 1;

                // Update line length
                cb->line_lengths[transaction.pos_line] = new_len;

                // Update lines
                cb->lines[transaction.pos_line] = (char32_t*)safe_realloc(cb->lines[transaction.pos_line], new_len * sizeof(char32_t));
                // Move the existing content after the position
                memmove(cb->lines[transaction.pos_line] + cb->line_lengths[transaction.pos_line],
                        cb->lines[transaction.pos_line + 1], cb->line_lengths[transaction.pos_line + 1] * sizeof(char32_t));
                // Free the old line
                free(cb->lines[transaction.pos_line + 1]);

                if (has_attributes) {
                    // Update attributes
                    cb->attributes[transaction.pos_line] = (CodeBufferCharAttributes*)safe_realloc(cb->attributes[transaction.pos_line],
                                                                                                   sizeof(CodeBufferCharAttributes) * new_len);
                    // Move the existing attributes after the position
                    memmove(cb->attributes[transaction.pos_line] + cb->line_lengths[transaction.pos_line],
                            cb->attributes[transaction.pos_line + 1],
                            cb->line_lengths[transaction.pos_line + 1] * sizeof(CodeBufferCharAttributes));
                    // Free the old attributes
                    free(cb->attributes[transaction.pos_line + 1]);

                    // Update node_lines
                    cb->node_lines[transaction.pos_line] = (CB_Node**)safe_realloc(cb->node_lines[transaction.pos_line],
                                                                                   sizeof(CB_Node*) * new_len);
                    // Move the existing node_lines after the position
                    memmove(cb->node_lines[transaction.pos_line] + cb->line_lengths[transaction.pos_line],
                            cb->node_lines[transaction.pos_line + 1],
                            cb->line_lengths[transaction.pos_line + 1] * sizeof(CB_Node*));
                    // Free the old node_lines
                    free(cb->node_lines[transaction.pos_line + 1]);
                }
                /* Shift lines up */
                for (size_t i = transaction.pos_line + 1; i < cb->line_count - 1; i++) {
                    cb->lines[i] = cb->lines[i + 1];
                    cb->line_lengths[i] = cb->line_lengths[i + 1];
                    cb->dirty_lines[i] = cb->dirty_lines[i + 1];
                    if (has_attributes) {
                        cb->attributes[i] = cb->attributes[i + 1];
                        cb->node_lines[i] = cb->node_lines[i + 1];
                    }
                }
                cb->line_count--;
                cb->lines = (char32_t**)safe_realloc(cb->lines, sizeof(char32_t*) * cb->line_count);
                cb->dirty_lines = (char*)safe_realloc(cb->dirty_lines, sizeof(char) * cb->line_count);
                cb->line_lengths = (size_t*)safe_realloc(cb->line_lengths, sizeof(size_t) * cb->line_count);
            }
            break;

        case TRANSACTION_SPLITLINE:
            if (transaction.pos_line >= cb->line_count) {
                fprintf(stderr, "SplitLine Error: Line number out of bounds\n");
                return;
            }
            if (transaction.pos_col > cb->line_lengths[transaction.pos_line]) {
                fprintf(stderr, "SplitLine Error: Column number out of bounds\n");
                return;
            }
            {
                size_t len = cb->line_lengths[transaction.pos_line];
                size_t first_part_len = transaction.pos_col;
                size_t second_part_len = len - transaction.pos_col;
                // Allocate new lines
                cb->lines = (char32_t**)safe_realloc(cb->lines, sizeof(char32_t*) * (cb->line_count + 1));
                /* Shift lines down */
                for (size_t i = cb->line_count; i > (size_t)(transaction.pos_line) + 1; i--) {
                    cb->lines[i] = cb->lines[i - 1];
                    cb->line_lengths[i] = cb->line_lengths[i - 1];
                    if (has_attributes) {
                        cb->attributes[i] = cb->attributes[i - 1];
                        cb->node_lines[i] = cb->node_lines[i - 1];
                    }
                }
                cb->line_count++;

                // Updates line and line length
                // Allocate memory for the second line
                cb->lines[transaction.pos_line + 1] = (char32_t*)malloc(second_part_len * sizeof(char32_t));
                // Check if memory allocation was successful
                if (!cb->lines[transaction.pos_line + 1]) {
                    perror("Failed to allocate memory for SplitLine second part");
                    exit(EXIT_FAILURE);
                }
                // Copy the second part of the line
                memcpy(cb->lines[transaction.pos_line + 1], cb->lines[transaction.pos_line] + transaction.pos_col, second_part_len * sizeof(char32_t));
                // Update the line length
                cb->line_lengths[transaction.pos_line + 1] = second_part_len;
                // Truncate the first part of the line with safe_realloc
                cb->lines[transaction.pos_line] = (char32_t*)safe_realloc(cb->lines[transaction.pos_line], first_part_len * sizeof(char32_t));
                cb->line_lengths[transaction.pos_line] = first_part_len;

                // Mark both lines as dirty
                cb->dirty_lines[transaction.pos_line] = 1; // Mark the first line as dirty
                cb->dirty_lines[transaction.pos_line + 1] = 1; // Mark the second line as dirty

                if (has_attributes) {
                    // Allocate memory for the attributes of the second line
                    cb->attributes[transaction.pos_line + 1] = (CodeBufferCharAttributes*)malloc(sizeof(CodeBufferCharAttributes) * second_part_len);
                    if (!cb->attributes[transaction.pos_line + 1]) {
                        perror("Failed to allocate memory for SplitLine second part attributes");
                        exit(EXIT_FAILURE);
                    }
                    // Copy the attributes of the second part of the line
                    memcpy(cb->attributes[transaction.pos_line + 1], cb->attributes[transaction.pos_line] + transaction.pos_col,
                           sizeof(CodeBufferCharAttributes) * second_part_len);
                    // Truncate the first part of the attributes with safe_realloc
                    cb->attributes[transaction.pos_line] = (CodeBufferCharAttributes*)safe_realloc(cb->attributes[transaction.pos_line],
                                                                                                  sizeof(CodeBufferCharAttributes) * first_part_len);

                    // Allocate memory for the node_lines of the second line
                    cb->node_lines[transaction.pos_line + 1] = (CB_Node**)malloc(sizeof(CB_Node*) * (second_part_len + 1));
                    if (!cb->node_lines[transaction.pos_line + 1]) {
                        perror("Failed to allocate memory for SplitLine second part node_lines");
                        exit(EXIT_FAILURE);
                    }
                    // Copy the node_lines of the second part of the line
                    memcpy(cb->node_lines[transaction.pos_line + 1], cb->node_lines[transaction.pos_line] + transaction.pos_col,
                           sizeof(CB_Node*) * second_part_len);
                    // Truncate the first part of the node_lines with safe_realloc
                    cb->node_lines[transaction.pos_line] = (CB_Node**)safe_realloc(cb->node_lines[transaction.pos_line],
                                                                                   sizeof(CB_Node*) * first_part_len);
                }
            }
            break;

        default:
            fprintf(stderr, "Unknown Transaction Type\n");
            break;
    }
}

/* Function to apply a single transaction */
void apply_transaction(CodeBuffer *cb, Transaction transaction) {
    // Add a transaction to the transaction list
    cb->transactions = (Transaction *)safe_realloc(cb->transactions, sizeof(Transaction) * (cb->transaction_count + 1));
    // Deep copy the transaction
    cb->transactions[cb->transaction_count].type = transaction.type;
    cb->transactions[cb->transaction_count].pos_line = transaction.pos_line;
    cb->transactions[cb->transaction_count].pos_col = transaction.pos_col;
    cb->transactions[cb->transaction_count].count = transaction.count;
    if (transaction.content) {
        cb->transactions[cb->transaction_count].content = strdup(transaction.content);
        if (!cb->transactions[cb->transaction_count].content) {
            perror("Failed to allocate memory for Delta transaction content");
            exit(EXIT_FAILURE);
        }
    } else {
        cb->transactions[cb->transaction_count].content = NULL;
    }
    // Increment number of Transactions
    cb->transaction_count++;

    // Apply the transaction
    base_apply_transaction(cb, transaction);
}

/* Function to take a snapshot of the buffer */
void snapshot(CodeBuffer *cb) {
    size_t i;

    cb->snapshot_lines = (char32_t**)malloc(sizeof(char32_t*) * cb->line_count);
    if (!cb->snapshot_lines) {
        perror("Failed to allocate memory for snapshot_lines");
        exit(EXIT_FAILURE);
    }
    cb->snapshot_line_lengths = (size_t *)malloc(sizeof(size_t) * cb->line_count);
    if (!cb->snapshot_line_lengths) {
        perror("Failed to allocate memory for snapshot_line_lengths");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < cb->line_count; i++) {
        cb->snapshot_lines[i] = (char32_t*)malloc(sizeof(char32_t) * (cb->line_lengths[i]));
        if (!cb->snapshot_lines[i]) {
            perror("Failed to allocate memory for a snapshot line");
            exit(EXIT_FAILURE);
        }
        memcpy(cb->snapshot_lines[i], cb->lines[i], sizeof(char32_t) * (cb->line_lengths[i]));
        //cb->snapshot_lines[i][cb->line_lengths[i]] = cb->line_lengths[i];
        cb->snapshot_line_lengths[i] = cb->line_lengths[i];
    }
    cb->snapshot_line_count = cb->line_count;
}

/* Function to copy the snapshot to the codebuffer */
void copy_snapshot_to_codebuffer(CodeBuffer *cb) {
    size_t i;

    if (!cb->snapshot_lines || cb->snapshot_line_count == 0) {
        fprintf(stderr, "No snapshot to copy\n");
        return;
    }

    // Free existing lines
    for (i = 0; i < cb->line_count; i++) {
        free(cb->lines[i]);
    }
    free(cb->lines);
    free(cb->line_lengths);
    cb->lines = NULL;
    cb->line_lengths = NULL;

    // Copy snapshot lines
    cb->lines = (char32_t**)malloc(sizeof(char32_t*) * cb->snapshot_line_count);
    if (!cb->lines) {
        perror("Failed to allocate memory for lines");
        exit(EXIT_FAILURE);
    }
    cb->line_lengths = (size_t *)malloc(sizeof(size_t) * cb->snapshot_line_count);
    if (!cb->line_lengths) {
        perror("Failed to allocate memory for line_lengths");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < cb->snapshot_line_count; i++) {
        cb->lines[i] = (char32_t*)malloc(sizeof(char32_t) * (cb->snapshot_line_lengths[i]));
        if (!cb->lines[i]) {
            perror("Failed to allocate memory for a line");
            exit(EXIT_FAILURE);
        }
        memcpy(cb->lines[i], cb->snapshot_lines[i], sizeof(char32_t) * (cb->snapshot_line_lengths[i]));
        cb->line_lengths[i] = cb->snapshot_line_lengths[i];
    }
    cb->line_count = cb->snapshot_line_count;

    /*  clear the attributes and node_lines */
    if (cb->attributes) {
        for (i = 0; i < cb->line_count; i++) {
            free(cb->attributes[i]);
        }
        free(cb->attributes);
        cb->attributes = NULL;
    }
    if (cb->node_lines) {
        for (i = 0; i < cb->line_count; i++) {
            free(cb->node_lines[i]);
        }
        free(cb->node_lines);
        cb->node_lines = NULL;
    }
}

/* Function to take a snapshot and get the delta */
Delta* snapshot_and_get_delta(CodeBuffer *cb) {
    size_t i;
    Delta *delta;

    if (!cb || cb->transaction_count == 0) {
        return NULL;
    }

    cb->change_version++;

    /* Allocate Delta */
    delta = (Delta *)malloc(sizeof(Delta));
    if (!delta) {
        perror("Failed to allocate memory for Delta");
        exit(EXIT_FAILURE);
    }

    delta->change_version = cb->change_version;
    delta->unique_document_id = strdup(cb->unique_document_id);
    if (!delta->unique_document_id) {
        perror("Failed to allocate memory for Delta unique_document_id");
        exit(EXIT_FAILURE);
    }
    delta->transaction_count = cb->transaction_count;
    delta->transactions = (Transaction *)malloc(sizeof(Transaction) * cb->transaction_count);
    if (!delta->transactions) {
        perror("Failed to allocate memory for Delta transactions");
        exit(EXIT_FAILURE);
    }

    /* Deep copy transactions */
    for (i = 0; i < cb->transaction_count; i++) {
        delta->transactions[i].type = cb->transactions[i].type;
        delta->transactions[i].pos_line = cb->transactions[i].pos_line;
        delta->transactions[i].pos_col = cb->transactions[i].pos_col;
        if (cb->transactions[i].content) {
            delta->transactions[i].content = strdup(cb->transactions[i].content);
            if (!delta->transactions[i].content) {
                perror("Failed to allocate memory for Delta transaction content");
                exit(EXIT_FAILURE);
            }
        } else {
            delta->transactions[i].content = NULL;
        }
        delta->transactions[i].count = cb->transactions[i].count;
    }

    /* Create a snapshot */
    snapshot(cb);

    /* Reset transactions */
    for (i = 0; i < cb->transaction_count; i++) {
        if (cb->transactions[i].content) {
            free(cb->transactions[i].content);
        }
    }
    free(cb->transactions);
    cb->transactions = NULL;
    cb->transaction_count = 0;

    return delta;
}

/* Function to replay a delta */
void replay_delta(CodeBuffer *cb, Delta *delta) {
    size_t i;

    if (!cb || !delta) return;

    if (delta->change_version != cb->change_version + 1) {
        fprintf(stderr, "Sync Error: Document version mismatch - expecting %d found %d\n",
                (int)(cb->change_version + 1), (int)delta->change_version);
        return;
    }

    if (strcmp(delta->unique_document_id, cb->unique_document_id) != 0) {
        fprintf(stderr, "Sync Error: Document ID mismatch\n");
        return;
    }

    /* Apply each transaction */
    for (i = 0; i < delta->transaction_count; i++) {
        base_apply_transaction(cb, delta->transactions[i]);
    }

    /* Update current_parent version */
    cb->change_version = delta->change_version;
}


/* Function to free a Delta */
void free_delta(Delta *delta) {
    size_t i;

    if (!delta) return;

    if (delta->unique_document_id) {
        free(delta->unique_document_id);
    }

    if (delta->transactions) {
        for (i = 0; i < delta->transaction_count; i++) {
            if (delta->transactions[i].content) {
                free(delta->transactions[i].content);
            }
        }
        free(delta->transactions);
    }

    free(delta);
}

/* Function to print the current_parent state of the buffer */
void print_code_buffer(CodeBuffer *cb) {
    size_t i;

    if (!cb) return;

    printf("CodeBuffer (Version: %d):\n", (int)cb->change_version);
    for (i = 0; i < cb->line_count; i++) {
        // Convert utf32 to utf8 for printing
        char *utf8_line = utf32_to_utf8(cb->lines[i], cb->line_lengths[i]);
        printf("%zu: %s\n", i, utf8_line);
        free(utf8_line);
    }
    printf("Highest Severity: %d\n", cb->highest_severity);
}


/* Function to free a CodeBuffer */
void free_code_buffer(CodeBuffer *cb) {
    size_t i;

    if (!cb) return;

    // Free unique_document_id
    if (cb->unique_document_id) {
        free(cb->unique_document_id);
        cb->unique_document_id = NULL;
    }

    // Free lines
    if (cb->lines) {
        for (i = 0; i < cb->line_count; i++) {
            free(cb->lines[i]);
        }
        free(cb->lines);
        cb->lines = NULL;
    }

    // Free line lengths
    if (cb->line_lengths) {
        free(cb->line_lengths);
        cb->line_lengths = NULL;
    }

    // Free dirty lines
    if (cb->dirty_lines) {
        free(cb->dirty_lines);
        cb->dirty_lines = NULL;
    }

    // Free parse_tree
    if (cb->parse_tree) cb_free_token_buffer(cb->parse_tree);
    cb->parse_tree = NULL;

    // Free attributes
    if (cb->attributes) {
        for (i = 0; i < cb->line_count; i++) {
            free(cb->attributes[i]);
        }
        free(cb->attributes);
        cb->attributes = NULL;
    }

    // Free node lines
    if (cb->node_lines) {
        for (i = 0; i < cb->line_count; i++) {
            free(cb->node_lines[i]);
        }
        free(cb->node_lines);
        cb->node_lines = NULL;
    }

    // Free snapshot lines
    if (cb->snapshot_lines) {
        for (i = 0; i < cb->snapshot_line_count; i++) {
            free(cb->snapshot_lines[i]);
        }
        free(cb->snapshot_lines);
        cb->snapshot_lines = NULL;
    }

    // Free snapshot line lengths
    if (cb->snapshot_line_lengths) {
        free(cb->snapshot_line_lengths);
    }
    cb->snapshot_line_lengths = NULL;

    // Free transactions
    if (cb->transactions) {
        for (i = 0; i < cb->transaction_count; i++) {
            if (cb->transactions[i].content) {
                free(cb->transactions[i].content);
            }
        }
        free(cb->transactions);
        cb->transactions = NULL;
    }

    free(cb);
}

// Function to return part of the code buffer based on the buffer position and length.
// It also sets the `line` and `col` position of the buffer (if they are not null, in which case they are ignored).
//
// If `value` is not NULL, it will set the value to the part of the buffer (caller must free() it) in utf8.
// Returns NULL if the buffer position is out of bounds, otherwise it returns a pointer into the buffer (utf32), not malloc'd
// and not null-terminated.
//
// Note this function and the library in general assume one newline character is in the original source at the end of each
// line, meaning one character per line is included in the length for buffer position calculations.
// The return value is a pointer to a line buffer, meaning that it ends at the end of the line, not at the end of the token.
// It is a null terminated string up to the line end and excluding the newline.
//
// Conversely, the 'value' parameter is populated with a null-terminated string that includes the token and ends at the
// end of the token and includes '\n' characters.
//
// In general, it is preferred that parsers split tokens on newline characters so that tokens don't span multiple lines.
char32_t* get_code_buffer_part(CodeBuffer *cb, size_t pos, size_t length, size_t *line, size_t *col, char** value) {
    if (!cb) return NULL;
    size_t current_line = 1;
    size_t current_col = 0;

    // Find the line / column position
    int i;
    for (i = 0; i < cb->line_count; i++) {
        size_t line_length = cb->line_lengths[i];
        if (pos >= current_col && pos <= current_col + line_length) {
            if (line) *line = current_line;
            if (col) *col = pos - current_col;
            if (value) {
                char32_t* value_utf32; // UTF-32 buffer for the value
                /* Get the text for the gap which might span multiple lines */
                /* Make value a buffer of the right size */
                value_utf32 = (char32_t *)malloc(sizeof(char32_t) * (length + 1));
                if (!value_utf32) {
                    perror("PANIC: Failed to allocate memory for get_code_buffer_part");
                    exit(EXIT_FAILURE);
                }
                /* Copy the text from the first line - we might need to copy from multiple lines */
                size_t length_to_go = length;
                size_t start_pos_for_line = pos - current_col;
                while (length_to_go > 0) {
                    size_t part_length = line_length - start_pos_for_line;
                    if (part_length > length_to_go) part_length = length_to_go;
                    
                    utf32_strncpy(value_utf32 + (length - length_to_go), cb->lines[i] + start_pos_for_line, part_length);
                    length_to_go -= part_length;
                    if (length_to_go > 0) {
                        /* Add line break */
                        value_utf32[length - length_to_go] = '\n';
                        length_to_go--;
                    }
                    if (length_to_go > 0) {
                        i++;
                        line_length = cb->line_lengths[i];
                        start_pos_for_line = 0;
                    }
                }
                // Convert to utf8
                *value = utf32_to_utf8(value_utf32, length);
            }
            return cb->lines[i] + (pos - current_col);
        }
        current_line++;
        current_col += line_length + 1; // Include the newline character
    }
    return NULL;
}

// Get code buffer total length (including a newline (0x0A) between each line)
size_t get_code_buffer_length(CodeBuffer *cb) {
    if (!cb) return 0;
    size_t length = 0;
    for (size_t i = 0; i < cb->line_count; i++) {
        length += cb->line_lengths[i] + 1; // Add a newline character
    }
    return length;
}

// Get code buffer total length when converted to UTF-8 format
size_t get_code_buffer_utf8_length(CodeBuffer *cb) {
    if (!cb) return 0;
    size_t length = 0;
    for (size_t i = 0; i < cb->line_count; i++) {
        length += utf32_utf8_length(cb->lines[i], cb->line_lengths[i]) + 1; // Add a newline character
    }
    return length;
}

// Function to extract the source code from the CodeBuffer in UTF-8 format
char* get_code_buffer_source(CodeBuffer *cb) {
    if (!cb || cb->line_count == 0) return NULL;

    size_t total_length = get_code_buffer_utf8_length(cb);
    char *source = (char *)malloc(total_length + 1); // +1 for null terminator
    if (!source) {
        perror("Failed to allocate memory for code buffer source");
        exit(EXIT_FAILURE);
    }

    size_t offset = 0;
    for (size_t i = 0; i < cb->line_count; i++) {
        char *utf8_line = utf32_to_utf8(cb->lines[i], cb->line_lengths[i]);
        size_t line_length = strlen(utf8_line);
        memcpy(source + offset, utf8_line, line_length);
        offset += line_length;
        source[offset++] = '\n'; // Add newline character
        free(utf8_line);
    }
    source[offset] = '\0'; // Null-terminate the string

    return source;
}
