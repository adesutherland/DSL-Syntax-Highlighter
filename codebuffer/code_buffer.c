/* code_buffer.c */

#include <unistd.h>
#include <ctype.h>

#include "dslsyntax_common.h"
#include "dslsyntax_log.h"
#include "thread_utils.h"

/* Common / Utility Functions */

// Helper function to calculate a rounded-up capacity for allocations.
// It rounds up to the next power of two to reduce realloc frequency.
static size_t calculate_capacity(size_t required_size) {
    if (required_size == 0) {
        return 0; // Or a small default like 8 if you want to pre-allocate for empty lines
    }

    // This is an efficient bit-twiddling algorithm to find the next power of two.
    size_t capacity = required_size;
    capacity--;
    capacity |= capacity >> 1;
    capacity |= capacity >> 2;
    capacity |= capacity >> 4;
    capacity |= capacity >> 8;
    capacity |= capacity >> 16;
    // For 64-bit systems
    if (sizeof(size_t) > 4) {
        capacity |= capacity >> 32;
    }
    capacity++;

    return capacity;
}

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
        LOG("Failed to allocate memory for utf8 string");
        return NULL;
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

/*
 * Utility to convert an Code string to utf8
 * This has been implemented to support multi-codepoint characters.
 * It returns a dynamically allocated UTF-8 string.
 */
char* line_to_utf8(const CodeBufferLine *line) {
    size_t utf8_length = 0;
    for (size_t i = 0; i < line->length; i++) {
        for (size_t j = 0; j < line->characters[i].codepoints; j++) {
            char32_t codepoint = line->characters[i].heap_character ? line->characters[i].heap_character[j] : line->characters[i].character[j];
            if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) {
                utf8_length += 1; // Invalid code point mapped to '?' (1 byte)
            } else if (codepoint < 0x80) {
                utf8_length++;
            } else if (codepoint < 0x800) {
                utf8_length += 2;
            } else if (codepoint < 0x10000) {
                utf8_length += 3;
            } else {
                utf8_length += 4;
            }
        }
    }

    char *utf8 = (char *)malloc(utf8_length + 1);
    if (!utf8) {
        LOG("Failed to allocate memory for utf8 string");
        return NULL;
    }

    size_t pos = 0;
    for (size_t i = 0; i < line->length; i++) {
        for (size_t j = 0; j < line->characters[i].codepoints; j++) {
            char32_t codepoint = line->characters[i].heap_character ? line->characters[i].heap_character[j] : line->characters[i].character[j];
            if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) {
                utf8[pos++] = '?'; // Invalid code point mapped to '?' (1 byte)
            } else if (codepoint < 0x80) {
                utf8[pos++] = (char)codepoint;
            } else if (codepoint < 0x800) {
                utf8[pos++] = (char)(0xC0 | (codepoint >> 6));
                utf8[pos++] = (char)(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                utf8[pos++] = (char)(0xE0 | (codepoint >> 12));
                utf8[pos++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                utf8[pos++] = (char)(0x80 | (codepoint & 0x3F));
            } else {
                utf8[pos++] = (char)(0xF0 | (codepoint >> 18));
                utf8[pos++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                utf8[pos++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                utf8[pos++] = (char)(0x80 | (codepoint & 0x3F));
            }
        }
    }
    utf8[pos] = '\0';

    return utf8;
}


/* Utility to convert an utf32 string to ascii (invalid characters are replaced with '?') */
char* utf32_to_ascii(const char32_t*utf32, size_t length) {
    char *ascii = (char *)malloc(length + 1);
    if (!ascii) {
        LOG("Failed to allocate memory for ascii string");
        return NULL;
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
        utf8_string = ""; // Handle NULL input by treating it as an empty string
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
        LOG("Failed to allocate memory for utf32 string");
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

/* Utility to convert a null terminated utf8 or ascii string to a line*/
/* Returns 0 on success, 1 on failure (e.g., memory allocation failure) */
int utf8_to_line(const char* utf8_string, CodeBufferLine *line) {
    if (!utf8_string) {
        /* Handle NULL input by treating it as an empty string */
        utf8_string = ""; // This is a common practice to avoid NULL dereference
    }

    // First pass: Calculate the number of UTF-32 codepoints.
    size_t utf32_length = 0;
    const char* temp_utf8_ptr = utf8_string;
    utf8_int32_t  decoded_code_point; // Variable to hold the decoded code point

    while (*temp_utf8_ptr) {
        // utf8codepoint() decodes the next code point and advances the pointer.
        temp_utf8_ptr = utf8codepoint(temp_utf8_ptr, &decoded_code_point);
        (void)decoded_code_point; // Suppress unused variable warning if needed
        utf32_length++;
    }

    // Allocate memory for the characters plus null terminator.
    line->characters = (CodeBufferCharacter*)malloc((utf32_length + 1) * sizeof(CodeBufferCharacter));
    if (!line->characters) {
        LOG("Failed to allocate memory for line");
        return 1; // Indicate allocation failure
    }

    // Second pass: Decode and copy the UTF-32 code points.
    size_t utf32_pos = 0;
    temp_utf8_ptr = utf8_string; // Reset pointer to the beginning

    while (*temp_utf8_ptr) {
        // Decode the next code point and store it.
        temp_utf8_ptr = utf8codepoint(temp_utf8_ptr, &decoded_code_point);
        line->characters[utf32_pos].character[0] = decoded_code_point;
        line->characters[utf32_pos].character[1] = 0; // Null-terminate the character
        line->characters[utf32_pos].heap_character = NULL; // Explicitly NULL
        line->characters[utf32_pos].codepoints = 1; // Set codepoints to 1 for each character
        line->characters[utf32_pos].token_type = LEXER_TOKEN;
        line->characters[utf32_pos].severity = CB_NONE; // Default severity
        line->characters[utf32_pos].subtree_type = LEXER_TOKEN; // Default subtree type
        line->characters[utf32_pos].subtree_lines = 0; // Default subtree lines
        line->characters[utf32_pos].node = NULL; // No parser node by default
        utf32_pos++;
    }

    // Null-terminate the UTF-32 string.
    line->characters[utf32_pos].character[0] = 0; // Null-terminate the last character
    line->characters[utf32_pos].heap_character = NULL; // Explicitly NULL
    line->characters[utf32_pos].codepoints = 0; // Set codepoints to 0 for the null terminator
    line->characters[utf32_pos].token_type = LEXER_WHITESPACE; // The last character is whitespace
    line->characters[utf32_pos].severity = CB_NONE; // Default severity for the last character
    line->characters[utf32_pos].subtree_type = 0; // No subtree type for the last character
    line->characters[utf32_pos].subtree_lines = 0; // No subtree lines for the last character
    line->characters[utf32_pos].node = NULL; // No parser node for the last character

    line->length = utf32_length;

    return 0; // Success
}

/* Common utility to safely reallocate a buffer
 * WARNING: Any new memory allocated because of an increasing size is not set to zero.
 */
void* safe_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        LOG("Failed to reallocate memory of size %zu", size);
        return NULL;
    }
    if (!ptr && new_ptr) {
        memset(new_ptr, 0, size);
    }
    return new_ptr;
}


/* Function to create a new CodeBuffer */
CodeBuffer* create_code_buffer(CommunicationFunctions *comm, ParserFunction parser_function) {
    CodeBuffer *cb = (CodeBuffer *)malloc(sizeof(CodeBuffer));
    if (!cb) {
        LOG("Failed to allocate memory for CodeBuffer");
        return NULL;
    }

    cb->communication_functions = comm;
    cb->parser_function = parser_function;
    cb->unique_document_id = NULL;
    cb->async_parse_active = 0;
    cb->parse_complete_pending = 0;
    cb->snapshot_number = 0;
    cb->snapshot_lines = NULL;
    cb->lines = NULL;
    cb->line_count = 0;
    cb->parse_tree = NULL;
    cb->highest_severity = CB_NONE;
    cb->ep_rules = NULL;

    // Out-of-process parser state initialization
    cb->parser_state = CB_PARSER_NOT_LOADED;
    cb->crash_count = 0;
    cb->auto_relaunch = 1;

    cb->transactions = NULL;
    cb->transaction_count = 0;
    cb->change_version = 0;

    return cb;
}

CB_ParserState cb_get_parser_state(CodeBuffer *cb) {
    if (!cb) return CB_PARSER_NOT_LOADED;
    return cb->parser_state;
}

void cb_set_auto_relaunch(CodeBuffer *cb, int enabled) {
    if (cb) {
        cb->auto_relaunch = enabled;
    }
}

/* Editor Functions */

/* Applying Transactions */

/* Function to apply a single transaction - this is the internal base functionality for applying and re-applying transactions*/
void base_apply_transaction(CodeBuffer *cb, Transaction transaction) {
    if (!cb) return;
    char32_t *utf32_content;
    size_t content_len;
    switch (transaction.type) {
        case TRANSACTION_ADDLINE:
            if (transaction.pos_line > cb->line_count) {
                LOG("AddLine Error: Line number %d out of bounds (count=%zu)", transaction.pos_line, cb->line_count);
                return;
            }
            // Lines
            CodeBufferLine *new_lines_add = (CodeBufferLine*)safe_realloc(cb->lines, sizeof(CodeBufferLine) * (cb->line_count + 1));
            if (!new_lines_add) return;
            cb->lines = new_lines_add;

            /* Shift lines down */
            for (size_t i = cb->line_count; i > (size_t)transaction.pos_line; i--) {
                cb->lines[i] = cb->lines[i - 1];
            }
            if (utf8_to_line(transaction.content, &(cb->lines[transaction.pos_line]))) {
                LOG("AddLine Error: Failed to convert content to line");
                return;
            }

            cb->line_count++;
            break;

        case TRANSACTION_DELETELINE:
            if (transaction.pos_line >= cb->line_count) {
                LOG("DeleteLine Error: Line number %d out of bounds (count=%zu)", transaction.pos_line, cb->line_count);
                return;
            }
            // Free the line characters being deleted
            free(cb->lines[transaction.pos_line].characters);
            /* Shift lines up */
            for (size_t i = transaction.pos_line; i < cb->line_count - 1; i++) {
                cb->lines[i] = cb->lines[i + 1];
            }
            // Update the line count
            cb->line_count--;

            if (cb->line_count > 0) {
                CodeBufferLine *new_lines_del = (CodeBufferLine*)safe_realloc(cb->lines, sizeof(CodeBufferLine) * cb->line_count);
                if (new_lines_del) cb->lines = new_lines_del;
                // If it fails, we keep the old larger buffer, which is fine for shrinking
            } else {
                free(cb->lines);
                cb->lines = NULL;
            }
            break;

        case TRANSACTION_ADDCHARS:
            if (transaction.pos_line >= cb->line_count) {
                LOG("AddChars Error: Line number %d out of bounds", transaction.pos_line);
                return;
            }
            if (transaction.pos_col > cb->lines[transaction.pos_line].length) {
                LOG("AddChars Error: Column number %d out of bounds", transaction.pos_col);
                return;
            }
            {
                size_t len = cb->lines[transaction.pos_line].length;
                utf32_content = utf8_to_utf32(transaction.content, &content_len);
                size_t new_len = len + content_len;
                cb->lines[transaction.pos_line].length = new_len;
                
                // Update the line
                CodeBufferCharacter *new_chars = (CodeBufferCharacter*)safe_realloc(cb->lines[transaction.pos_line].characters, (new_len + 1) * sizeof(CodeBufferCharacter));
                if (!new_chars) return;
                cb->lines[transaction.pos_line].characters = new_chars;
                
                // Get the source character for attributes (safely)
                CodeBufferCharacter attr_char;
                if (transaction.pos_col > 0) {
                    /* Prefer left neighbor for continuation */
                    attr_char = cb->lines[transaction.pos_line].characters[transaction.pos_col - 1];
                } else if (transaction.pos_col < len) {
                    /* Fallback to right neighbor if at start of line */
                    attr_char = cb->lines[transaction.pos_line].characters[transaction.pos_col];
                } else {
                    /* Default for empty line */
                    attr_char.codepoints = 0;
                    attr_char.heap_character = NULL;
                    attr_char.token_type = LEXER_WHITESPACE;
                    attr_char.severity = CB_NONE;
                    attr_char.subtree_type = 0;
                    attr_char.subtree_lines = 0;
                    attr_char.node = NULL;
                }

                // Move the existing content after the position
                memmove(cb->lines[transaction.pos_line].characters + transaction.pos_col + content_len, cb->lines[transaction.pos_line].characters + transaction.pos_col,
                        (len - transaction.pos_col + 1) * sizeof(CodeBufferCharacter));
                // insert the new content at the position
                for (size_t i = transaction.pos_col; i < transaction.pos_col + content_len; i++) {
                    CodeBufferCharacter* cb_char = &cb->lines[transaction.pos_line].characters[i];
                    char32_t cp = utf32_content[i - transaction.pos_col];
                    cb_char->character[0] = cp;
                    cb_char->character[1] = 0; // Null-terminate the character
                    cb_char->codepoints = 1; // Set codepoints to 1 for each character
                    
                    /* Determine if we are extending the previous token */
                    int extend = 0;
                    if (!utf32_isspace(cp) && attr_char.token_type != LEXER_WHITESPACE && attr_char.token_type != LEXER_TOKEN) {
                        if (attr_char.token_type == LEXER_STRING_LITERAL || attr_char.token_type == LEXER_COMMENT) {
                            extend = 1; /* Always extend strings/comments */
                        } else if (isalnum(cp) || cp == '_') {
                            /* Extend if it's alphanumeric/underscore AND the previous character is part of a word-like token */
                            if (attr_char.token_type == LEXER_IDENTIFIER || attr_char.token_type == LEXER_KEYWORD || attr_char.token_type == LEXER_NUMBER_LITERAL) {
                                extend = 1;
                            }
                        } else if (ispunct(cp) && cp != '"' && cp != '\'' && cp != '#' && cp != '/') {
                             if (attr_char.token_type == LEXER_OPERATOR || attr_char.token_type == LEXER_OPERATOR_ASSIGN || attr_char.token_type == LEXER_OPERATOR_ARITHMETIC || attr_char.token_type == LEXER_OPERATOR_LOGICAL) {
                                 extend = 1;
                             }
                        }
                    }

                    if (utf32_isspace(cp)) {
                        cb_char->token_type = LEXER_WHITESPACE;
                        cb_char->severity = CB_NONE;
                        cb_char->node = NULL;
                    } else if (extend) {
                        cb_char->token_type = attr_char.token_type;
                        cb_char->severity = attr_char.severity;
                        cb_char->node = attr_char.node;
                    } else {
                        cb_char->token_type = LEXER_TOKEN;
                        cb_char->severity = CB_NONE;
                        cb_char->node = NULL;
                    }
                    
                    cb_char->heap_character = NULL; // New character doesn't have heap allocation
                    cb_char->subtree_type = 0;
                    cb_char->subtree_lines = 0;
                }
                // Free the utf32_content
                free(utf32_content);
            }
            break;

        case TRANSACTION_DELETECHARS:
            if (transaction.pos_line >= cb->line_count) {
                LOG("DeleteChars Error: Line number %d out of bounds", transaction.pos_line);
                return;
            }
            if (transaction.pos_col + transaction.count > cb->lines[transaction.pos_line].length) {
                LOG("DeleteChars Error: Column and count out of bounds");
                return;
            }
            {
                CodeBufferCharacter* chars = cb->lines[transaction.pos_line].characters;
                size_t len = cb->lines[transaction.pos_line].length;
                size_t new_len = len - transaction.count;
                // Move the existing content after the position
                if (transaction.pos_col + transaction.count < len) {
                    memmove(chars + transaction.pos_col, chars + transaction.pos_col + transaction.count,
                            (len - transaction.pos_col - transaction.count + 1) * sizeof(CodeBufferCharacter));
                }
                // Shrink the line
                CodeBufferCharacter *new_chars = (CodeBufferCharacter*)safe_realloc(chars, (new_len + 1) * sizeof(CodeBufferCharacter));
                if (new_chars) cb->lines[transaction.pos_line].characters = new_chars;
                // If it fails, we keep the old larger buffer, which is fine for shrinking

                // Update the line length
                cb->lines[transaction.pos_line].length = new_len;
            }
            break;

        case TRANSACTION_JOINLINES:
            if (transaction.pos_line < 0 || transaction.pos_line + 1 >= (int)cb->line_count) {
                LOG("JoinLines Error: Line number %d out of bounds", transaction.pos_line);
                return;
            }
            {
                size_t new_len = cb->lines[transaction.pos_line].length + cb->lines[transaction.pos_line + 1].length;
                // Update lines
                CodeBufferCharacter *new_chars = (CodeBufferCharacter*)safe_realloc(cb->lines[transaction.pos_line].characters, (new_len + 1) * sizeof(CodeBufferCharacter));
                if (!new_chars) {
                    LOG("JoinLines Error: Failed to reallocate memory for joined line");
                    return;
                }
                cb->lines[transaction.pos_line].characters = new_chars;

                // Move the existing content after the position
                memmove(cb->lines[transaction.pos_line].characters + cb->lines[transaction.pos_line].length,
                        cb->lines[transaction.pos_line + 1].characters, (cb->lines[transaction.pos_line + 1].length + 1) * sizeof(CodeBufferCharacter));
                // Free the old line
                free(cb->lines[transaction.pos_line + 1].characters);

                // Update line length
                cb->lines[transaction.pos_line].length = new_len;

                /* Shift lines up */
                for (size_t i = transaction.pos_line + 1; i < cb->line_count - 1; i++) {
                    cb->lines[i] = cb->lines[i + 1];
                }
                cb->line_count--;
                cb->lines = (CodeBufferLine*)safe_realloc(cb->lines, sizeof(CodeBufferLine) * cb->line_count);
            }
            break;

        case TRANSACTION_SPLITLINE:
            if (transaction.pos_line >= cb->line_count) {
                LOG("SplitLine Error: Line number %d out of bounds", transaction.pos_line);
                return;
            }
            if (transaction.pos_col > cb->lines[transaction.pos_line].length) {
                LOG("SplitLine Error: Column number %d out of bounds", transaction.pos_col);
                return;
            }
            {
                size_t len = cb->lines[transaction.pos_line].length;
                size_t first_part_len = transaction.pos_col;
                size_t second_part_len = len - transaction.pos_col;
                // Allocate new lines
                CodeBufferLine *new_lines = (CodeBufferLine*)safe_realloc(cb->lines, sizeof(CodeBufferLine) * (cb->line_count + 1));
                if (!new_lines) {
                    LOG("SplitLine Error: Failed to reallocate lines array");
                    return;
                }
                cb->lines = new_lines;

                /* Shift lines down */
                for (size_t i = cb->line_count; i > (size_t)(transaction.pos_line) + 1; i--) {
                    cb->lines[i] = cb->lines[i - 1];
                }
                cb->line_count++;

                // Updates line and line length
                // Allocate memory for the second line
                cb->lines[transaction.pos_line + 1].characters = (CodeBufferCharacter*)malloc((second_part_len + 1) * sizeof(CodeBufferCharacter));
                if (!cb->lines[transaction.pos_line + 1].characters) {
                    LOG("Failed to allocate memory for SplitLine second part");
                    /* We already incremented line_count and shifted, but memory failed. 
                       This leaves the buffer in an inconsistent state. 
                       In a real production system we'd rollback, but for now we just return. */
                    return;
                }
                // Copy the second part of the line
                memcpy(cb->lines[transaction.pos_line + 1].characters, cb->lines[transaction.pos_line].characters + transaction.pos_col, (second_part_len + 1) * sizeof(CodeBufferCharacter));
                // Update the line length
                cb->lines[transaction.pos_line + 1].length = second_part_len;
                // Truncate the first part of the line with safe_realloc
                cb->lines[transaction.pos_line].characters = (CodeBufferCharacter*)safe_realloc(cb->lines[transaction.pos_line].characters, (first_part_len + 1) * sizeof(CodeBufferCharacter));
                cb->lines[transaction.pos_line].length = first_part_len;
                // Zero the last character of the first part
                cb->lines[transaction.pos_line].characters[first_part_len].character[0] = 0; // Null-terminate the character
                cb->lines[transaction.pos_line].characters[first_part_len].codepoints = 0; // Set codepoints to 0 for the null terminator
                cb->lines[transaction.pos_line].characters[first_part_len].token_type = LEXER_WHITESPACE; // The last character is whitespace
                cb->lines[transaction.pos_line].characters[first_part_len].severity = CB_NONE; // Default severity for the last character
                cb->lines[transaction.pos_line].characters[first_part_len].subtree_type = 0; // No subtree type for the last character
                cb->lines[transaction.pos_line].characters[first_part_len].subtree_lines = 0; // No subtree lines for the last character
                cb->lines[transaction.pos_line].characters[first_part_len].node = NULL; // No parser node for the last character
            }
            break;

        default:
            LOG("Unknown Transaction Type: %c", transaction.type);
            break;
    }

    /* Call Emergency Parser to update the parse tree structure heuristically */
    cb_emergency_parse_transaction(cb, transaction);
}

/* Function to apply a single transaction */
void editor_apply_transaction(CodeBuffer *cb, Transaction transaction) {
    int locked = 0;

    if (!cb) return;
    if (parser_thread_utils_is_initialized()) {
        if (enter_codeblock_critical_section() == 0) {
            locked = 1;
        } else {
            LOG("editor_apply_transaction: failed to enter critical section");
            return;
        }
    }

    // Add a transaction to the transaction list
    Transaction *new_transactions = (Transaction *)safe_realloc(cb->transactions, sizeof(Transaction) * (cb->transaction_count + 1));
    if (!new_transactions) {
        LOG("Failed to reallocate memory for transactions");
        goto done;
    }
    cb->transactions = new_transactions;

    // Deep copy the transaction
    cb->transactions[cb->transaction_count].type = transaction.type;
    cb->transactions[cb->transaction_count].pos_line = transaction.pos_line;
    cb->transactions[cb->transaction_count].pos_col = transaction.pos_col;
    cb->transactions[cb->transaction_count].count = transaction.count;
    if (transaction.content) {
        cb->transactions[cb->transaction_count].content = strdup(transaction.content);
        if (!cb->transactions[cb->transaction_count].content) {
            LOG("Failed to allocate memory for Delta transaction content");
            goto done;
        }
    } else {
        cb->transactions[cb->transaction_count].content = NULL;
    }
    // Increment number of Transactions
    cb->transaction_count++;

    // Apply the transaction
    base_apply_transaction(cb, transaction);

done:
    if (locked) exit_codeblock_critical_section();
}

/* Function to take a snapshot of the buffer */
void snapshot(CodeBuffer *cb) {
    size_t i;

    if (cb->snapshot_lines) {
        for (i = 0; i < cb->snapshot_line_count; i++) {
            if (cb->snapshot_lines[i].characters) {
                for (size_t j = 0; j <= cb->snapshot_lines[i].length; j++) {
                    if (cb->snapshot_lines[i].characters[j].heap_character) {
                        free(cb->snapshot_lines[i].characters[j].heap_character);
                    }
                }
                free(cb->snapshot_lines[i].characters);
            }
        }
        free(cb->snapshot_lines);
    }
    cb->snapshot_lines = (CodeBufferLine*)malloc(sizeof(CodeBufferLine) * cb->line_count);
    if (!cb->snapshot_lines) {
        LOG("Failed to allocate memory for snapshot_lines");
        return;
    }

    for (i = 0; i < cb->line_count; i++) {
        cb->snapshot_lines[i].characters = (CodeBufferCharacter*)malloc(sizeof(CodeBufferCharacter) * (cb->lines[i].length + 1));
        if (!cb->snapshot_lines[i].characters) {
            LOG("Failed to allocate memory for a snapshot line");
            return;
        }
        memcpy(cb->snapshot_lines[i].characters, cb->lines[i].characters, sizeof(CodeBufferCharacter) * (cb->lines[i].length + 1));
        cb->snapshot_lines[i].length = cb->lines[i].length;
    }
    cb->snapshot_line_count = cb->line_count;
}

/* Function to copy the snapshot to the codebuffer */
void copy_snapshot_to_codebuffer(CodeBuffer *cb) {
    size_t i;

    if (!cb->snapshot_lines || cb->snapshot_line_count == 0) {
        LOG("No snapshot to copy");
        return;
    }

    // Free existing lines
    if (cb->lines) {
        for (i = 0; i < cb->line_count; i++) {
            if (cb->lines[i].characters) {
                for (size_t j = 0; j <= cb->lines[i].length; j++) {
                    if (cb->lines[i].characters[j].heap_character) {
                        free(cb->lines[i].characters[j].heap_character);
                    }
                }
                free(cb->lines[i].characters);
            }
        }
        free(cb->lines);
    }

    // Copy snapshot line
    cb->lines = cb->snapshot_lines;
    cb->line_count = cb->snapshot_line_count;
    cb->snapshot_lines = NULL; // Clear the snapshot lines
    cb->snapshot_line_count = 0; // Reset the snapshot line count
}

/* Function to take a snapshot and get the delta */
Delta* snapshot_and_get_delta(CodeBuffer *cb) {
    size_t i;
    Delta *delta;

    if (!cb || cb->transaction_count == 0) {
        return NULL;
    }

    delta = (Delta *)malloc(sizeof(Delta));
    if (!delta) {
        LOG("Failed to allocate memory for Delta");
        return NULL;
    }

    delta->unique_document_id = cb->unique_document_id ? strdup(cb->unique_document_id) : NULL;
    delta->base_version = cb->change_version;
    delta->change_version = cb->change_version + 1;
    delta->overlay_id = NULL;
    cb->change_version = delta->change_version;

    /* Transactions -> Delta */
    delta->transaction_count = cb->transaction_count;
    delta->transactions = cb->transactions;
    cb->transactions = NULL;
    cb->transaction_count = 0;

    // Create a snapshot
    snapshot(cb);

    return delta;
}

/* Function to replay a delta */
void base_replay_delta(CodeBuffer *cb, Delta *delta) {
    size_t i;

    if (!cb || !delta) return;

    if (delta->unique_document_id && cb->unique_document_id &&
        strcmp(delta->unique_document_id, cb->unique_document_id) != 0) {
        LOG("Sync Error: Document mismatch - buffer=%s delta=%s",
                cb->unique_document_id, delta->unique_document_id);
        return;
    }

    if (delta->change_version != cb->change_version + 1) {
        LOG("Sync Error: Document version mismatch - expecting %d found %d",
                (int)(cb->change_version + 1), (int)delta->change_version);
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
    if (delta->overlay_id) {
        free(delta->overlay_id);
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
        char *utf8_line = line_to_utf8(&(cb->lines[i]));
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
            for (size_t j = 0; j <= cb->lines[i].length; j++) {
                if (cb->lines[i].characters[j].heap_character) {
                    free(cb->lines[i].characters[j].heap_character);
                }
            }
            free(cb->lines[i].characters);
        }
        free(cb->lines);
        cb->lines = NULL;
    }

    // Free parse_tree
    if (cb->parse_tree) cb_free_token_buffer(cb->parse_tree);
    cb->parse_tree = NULL;

    // Free snapshot lines
    if (cb->snapshot_lines) {
        for (i = 0; i < cb->snapshot_line_count; i++) {
            free(cb->snapshot_lines[i].characters);
        }
        free(cb->snapshot_lines);
        cb->snapshot_lines = NULL;
    }

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
// and as part of the CodeBufferLine structure ends at the end of the line.
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
CodeBufferCharacter* get_code_buffer_part(CodeBuffer *cb, size_t pos, size_t length, size_t *line, size_t *col, char** value) {
    if (!cb) return NULL;
    size_t current_line, current_col;
    size_t current_pos = 0;
    size_t found_line = 0;
    size_t found_col = 0;
    char found = 0;
    CodeBufferCharacter* found_char = NULL;

    // Find the line / column position
    for (current_line = 0; current_line < cb->line_count; current_line++) {
        size_t line_length = cb->lines[current_line].length + 1; // Include the newline character
        if (pos >= current_pos && pos < current_pos + line_length) {
            found_line = current_line;
            found_col = pos - current_pos;
            found_char = cb->lines[current_line].characters + found_col;
            found = 1; // Found the character
            break;
        }
        current_pos += line_length;
    }

    if (value && found) {
        // We need to get the value part as UTF8 spanning lines as needed

        // First, we have to determine the length of the value by walking through the characters in the lines
        // and get the codepoints, convert determine each utf8 length
        current_col = found_col;
        current_line = found_line;
        size_t length_to_go = length;
        size_t value_length = 0;
        while (length_to_go > 0) {
            // Get the character at the position in the line
            CodeBufferCharacter *c = &cb->lines[current_line].characters[current_col];
            char32_t *codepoint = c->heap_character ? c->heap_character : c->character;
            value_length += utf32_utf8_length(codepoint, c->codepoints);
            // Move to the next character
            current_col++;
            length_to_go--;
            // Check and move to the next line if needed
            if (current_col >= cb->lines[current_line].length) {
                current_col = 0;
                current_line++;
                // If we are at the end of the line, we need to add a newline character
                if (length_to_go > 0) {
                    value_length++; // Add a newline character
                    length_to_go--;
                }
                if (current_line >= cb->line_count) break; // No more lines
            }
        }

        // UTF-8 buffer for the value
        *value = (char *)malloc((value_length + 1) * sizeof(char)); // +1 for null terminator
        if (!*value) {
            LOG("Failed to allocate memory for value");
            return NULL;
        }

        /* Now we need to copy the characters from the lines into the value_utf32 buffer */
        current_col = found_col;
        current_line = found_line;
        length_to_go = length;
        size_t write_pos = 0; // Now the position we are at in the value_utf32 buffer
        while (length_to_go > 0) {
            // Get the character at the position in the line
            CodeBufferCharacter *c = &cb->lines[current_line].characters[current_col];
            char32_t *codepoint = c->heap_character ? c->heap_character : c->character;
            for (size_t i = 0; i < c->codepoints; i++) {
                // Create the utf8 string
                write_pos += utf32_to_utf8_char(codepoint[i], *value + write_pos, value_length - write_pos);
            }

            // Move to the next character
            current_col++;
            length_to_go--;
            // Check and move to the next line if needed
            if (current_col >= cb->lines[current_line].length) {
                current_col = 0;
                current_line++;
                // If we are at the end of the line, we need to add a newline character
                if (length_to_go > 0) {
                    (*value)[write_pos++] = '\n';
                    length_to_go--;
                }
                if (current_line >= cb->line_count) break; // No more lines
            }
        }
        (*value)[write_pos] = '\0'; // Null-terminate the string
    }

    if (found) {
        if (line) *line = found_line + 1; // Convert to 1-based index
        if (col) *col = found_col;
    }
    return found_char;
}

// Get code buffer total length (including a newline (0x0A) between each line)
size_t get_code_buffer_length(CodeBuffer *cb) {
    if (!cb) return 0;
    size_t length = 0;
    for (size_t i = 0; i < cb->line_count; i++) {
        length += cb->lines[i].length + 1; // Add a newline character
    }
    return length;
}

// Get code buffer total length when converted to UTF-8 format
size_t get_code_buffer_utf8_length(CodeBuffer *cb) {
    if (!cb) return 0;
    size_t length = 0;
    for (size_t i = 0; i < cb->line_count; i++) {
        for (size_t j = 0; j < cb->lines[i].length; j++) {
            char32_t codepoint = cb->lines[i].characters[j].character[0];
            if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) {
                length += 1; // Invalid code point mapped to '?' (1 byte)
            } else if (codepoint < 0x80) {
                length++;
            } else if (codepoint < 0x800) {
                length += 2;
            } else if (codepoint < 0x10000) {
                length += 3;
            } else {
                length += 4;
            }
        }
        length++; // Add a newline character for each line
    }
    return length;
}

// Function to extract the source code from the CodeBuffer in UTF-8 format
char* get_code_buffer_source(CodeBuffer *cb) {
    if (!cb || cb->line_count == 0) return NULL;

    size_t total_length = get_code_buffer_utf8_length(cb);
    char *source = (char *)malloc(total_length + 1); // +1 for null terminator
    if (!source) {
        LOG("Failed to allocate memory for code buffer source");
        return NULL;
    }

    size_t offset = 0;
    for (size_t i = 0; i < cb->line_count; i++) {
        for (size_t j = 0; j < cb->lines[i].length; j++) {
            char32_t codepoint = cb->lines[i].characters[j].character[0];
            if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) {
                source[offset++] = '?'; // Invalid code point mapped to '?' (1 byte)
            } else if (codepoint < 0x80) {
                source[offset++] = (char)codepoint;
            } else if (codepoint < 0x800) {
                source[offset++] = (char)(0xC0 | (codepoint >> 6));
                source[offset++] = (char)(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                source[offset++] = (char)(0xE0 | (codepoint >> 12));
                source[offset++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                source[offset++] = (char)(0x80 | (codepoint & 0x3F));
            } else {
                source[offset++] = (char)(0xF0 | (codepoint >> 18));
                source[offset++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                source[offset++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                source[offset++] = (char)(0x80 | (codepoint & 0x3F));
            }
        }
        source[offset++] = '\n'; // Add newline character
    }
    source[offset] = '\0'; // Null-terminate the string

    return source;
}

#define MAX_STACK_CHARS 256

void cb_sync_line(CodeBuffer *cb, int line_index, const char *new_text) {
    int locked = 0;

    if (!cb) return;
    if (parser_thread_utils_is_initialized()) {
        if (enter_codeblock_critical_section() == 0) {
            locked = 1;
        } else {
            LOG("cb_sync_line: failed to enter critical section");
            return;
        }
    }
    if (line_index < 0 || line_index >= (int)cb->line_count) goto done;

    CodeBufferLine *line = &cb->lines[line_index];
    size_t old_len = line->length;

    char32_t stack_old_cp[MAX_STACK_CHARS];
    char32_t *old_cp = old_len <= MAX_STACK_CHARS ? stack_old_cp : malloc(old_len * sizeof(char32_t));
    if (!old_cp) goto done;

    for (size_t i = 0; i < old_len; i++) {
        old_cp[i] = line->characters[i].heap_character ? line->characters[i].heap_character[0] : line->characters[i].character[0];
    }

    size_t new_len = 0;
    const char *temp = new_text ? new_text : "";
    utf8_int32_t cp;
    while (*temp) {
        temp = (const char *)utf8codepoint((const utf8_int8_t *)temp, &cp);
        new_len++;
    }

    char32_t stack_new_cp[MAX_STACK_CHARS];
    char32_t *new_cp = new_len <= MAX_STACK_CHARS ? stack_new_cp : malloc(new_len * sizeof(char32_t));
    if (!new_cp) {
        if (old_cp != stack_old_cp) free(old_cp);
        goto done;
    }

    temp = new_text ? new_text : "";
    for (size_t i = 0; i < new_len; i++) {
        temp = (const char *)utf8codepoint((const utf8_int8_t *)temp, &cp);
        new_cp[i] = (char32_t)cp;
    }

    size_t prefix_len = 0;
    while (prefix_len < old_len && prefix_len < new_len && old_cp[prefix_len] == new_cp[prefix_len]) {
        prefix_len++;
    }

    size_t suffix_len = 0;
    while (suffix_len < old_len - prefix_len && suffix_len < new_len - prefix_len &&
           old_cp[old_len - 1 - suffix_len] == new_cp[new_len - 1 - suffix_len]) {
        suffix_len++;
    }

    size_t del_count = old_len - prefix_len - suffix_len;
    if (del_count > 0) {
        Transaction del_txn = { TRANSACTION_DELETECHARS, line_index, (int)prefix_len, NULL, (int)del_count };
        editor_apply_transaction(cb, del_txn);
    }

    size_t add_count = new_len - prefix_len - suffix_len;
    if (add_count > 0) {
        const char *add_start = new_text ? new_text : "";
        for (size_t i = 0; i < prefix_len; i++) {
            add_start = (const char *)utf8codepoint((const utf8_int8_t *)add_start, &cp);
        }
        const char *add_end = add_start;
        for (size_t i = 0; i < add_count; i++) {
            add_end = (const char *)utf8codepoint((const utf8_int8_t *)add_end, &cp);
        }
        
        size_t utf8_add_len = add_end - add_start;
        char stack_add_str[MAX_STACK_CHARS * 4];
        char *add_str = utf8_add_len < sizeof(stack_add_str) ? stack_add_str : malloc(utf8_add_len + 1);
        if (add_str) {
            strncpy(add_str, add_start, utf8_add_len);
            add_str[utf8_add_len] = '\0';
            Transaction add_txn = { TRANSACTION_ADDCHARS, line_index, (int)prefix_len, add_str, 0 };
            editor_apply_transaction(cb, add_txn);
            if (add_str != stack_add_str) free(add_str);
        }
    }

    if (old_cp != stack_old_cp) free(old_cp);
    if (new_cp != stack_new_cp) free(new_cp);

done:
    if (locked) exit_codeblock_critical_section();
}
