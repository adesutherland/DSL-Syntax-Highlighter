//
// Created by Adrian Sutherland on 11/10/2024.
//
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
typedef HANDLE ThreadType;
typedef HANDLE MutexType; /* This is for the global critical section mutex */
/* Define ThreadFunctionType to match Windows CreateThread */
typedef DWORD (WINAPI *ThreadFunctionType)(LPVOID lpThreadParameter);
#else /* POSIX (Linux, macOS) */
#include <unistd.h>
#endif

#include "token_buffer.h"

#define CTRL_KEY(k) ((k) & 0x1f)
#define MAX_LINES 1000
#define MAX_LINE_LENGTH 1024
#define FOOTER_TEXT " Toy Editor  -  Ctrl-Q to quit  -  Ctrl-S to save"
#define HEADER_TEXT " File: %s"

// Colour pairs
#define PAIR_HEADER 1
#define PAIR_FOOTER 2
#define PAIR_BODY 3
#define PAIR_COMMENT 4
#define PAIR_KEYWORD 5
#define PAIR_STRING 6
#define PAIR_NUM 7
#define PAIR_OPERATOR 8
#define PAIR_VARIABLE 9
#define PAIR_ERROR 10
#define PAIR_INFOMESSAGE 11
#define PAIR_WARNINGMESSAGE 12
#define PAIR_ERRORMESSAGE 13
#define ATTR_UNDERLINE 16
#define ATTR_BOLD 32
#define ATTR_ITALIC 64
#define ATTR_DIM 128

// Severity levels
#define SEVERITY_INFO 0
#define SEVERITY_WARNING 1
#define SEVERITY_ERROR 2

typedef struct TextBuffer {
    int num_rows;
    char **rows;
    unsigned char **row_syntax; // Syntax highlighting - contains the type of each character corresponding to PAIR_* colours
    unsigned char **message_number; // Syntax highlighting - contains the message number of each character
} TextBuffer;

typedef struct ErrorMessage {
    char *text;
    char severity;
} ErrorMessage;

// Message array
ErrorMessage error_messages[256];

// For the file name
char loaded_filename[256];

// Scroll position - line and column
int scroll_line = 0;
int scroll_col = 0;

static void cross_platform_sleep_ms(int milliseconds) {
    if (milliseconds <= 0) return;
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

/*
 * A polled ncurses getch() replacement that also monitors a cross-platform event.
 * It unlocks the global critical section before polling/waiting and re-locks it before returning.
 *
 * @return The character code if a key is pressed,
 *         GETCH_EVENT_RAISED if event_source_active was true and the event was raised,
 *         ERR (from curses.h) on timeout or error.
 */
#define GETCH_EVENT_RAISED (-2)
int getch_or_parse_event() {
    int result_char = ERR;
    long long start_time_ms = 0;
    int event_detected_flag = 0; /* Flag to indicate if the event was detected during polling */
    WINDOW *win = stdscr;

    // This gives access to any parsing thread
    if (exit_codeblock_critical_section() != 0) {
        fprintf(stderr, "polled_getch: Failed to exit CS!\n");
        enter_codeblock_critical_section(); /* Attempt to re-enter to maintain consistency */
        return ERR;
    }

    if (!editor_is_parsing_thread_active()) {
        /* Optimized path: No active event source, just call wgetch directly */
        result_char = wgetch(win);
    } else {
        /* Polling path: Event source is active, perform polling. */
        for (;;) { /* Polling loop */
            /* Check for the event */
            if (check_parse_complete_event() == 1) {
                result_char = GETCH_EVENT_RAISED;
                event_detected_flag = 1;
                break;
            }

            /* Check for ncurses input (non-blockingly for this poll iteration) */
            nodelay(win, TRUE);
            int ch_input = wgetch(win);
            nodelay(win, FALSE); /* Restore blocking behavior for future calls by other parts */

            if (ch_input != ERR) {
                result_char = ch_input;
                break;
            }

            /* Sleep */
            cross_platform_sleep_ms(20);
        }
    }

    // This takes control for this editor main thread
    if (enter_codeblock_critical_section() != 0) {
        fprintf(stderr, "polled_getch: CRITICAL - Failed to re-enter CS!\n");
        /* State is potentially inconsistent. */
    }

    return result_char;
}

// Convert the CB_ParseTree token type to a highlight code
// 4 least significant bits are used for the pair code
// 5th bit used to indicate whether the token is underlined (16)
// 6th bit used to indicate whether the token is bold (32)
// 7th bit used to indicate whether the token is italicized (64)
// 8th bit used to indicate whether the token is dimmed (128)
unsigned char token_to_highlight(CB_Node *token) {
    CB_NodeType type = token->type;
    unsigned char highlight;
    switch (type) {
        case LEXER_COMMENT:
            highlight = PAIR_COMMENT + ATTR_DIM;
            break;
        case LEXER_KEYWORD:
            highlight = PAIR_KEYWORD;
            break;
        case LEXER_STRING_LITERAL:
            highlight = PAIR_STRING;
            break;
        case LEXER_NUMBER_LITERAL:
            highlight = PAIR_NUM;
            break;
        case LEXER_OPERATOR:
        case LEXER_OPERATOR_ASSIGN:
        case LEXER_OPERATOR_ARITHMETIC:
        case LEXER_OPERATOR_LOGICAL:
        case LEXER_LH_EXPR:
        case LEXER_RH_EXPR:
            highlight = PAIR_OPERATOR;
            break;
        case LEXER_IDENTIFIER:
            highlight = PAIR_VARIABLE;
            break;
        case SYNTAX_ERROR:
            highlight = PAIR_ERROR;
            break;
        default:
            highlight = PAIR_BODY;
    }
    if (token->message) {
        highlight = highlight + ATTR_UNDERLINE;
    }
    return highlight;
}

// Add a message to the error_messages array, finding an empty slot and returning the
// index is one base so that 0 can be used as a null value
unsigned char add_message(const char *text, char severity) {
    for (int i = 0; i < 256; i++) {
        if (!error_messages[i].text) {
            error_messages[i].text = strdup(text);
            error_messages[i].severity = severity;
            return i + 1;
        }
    }
    return 0;
}

// Clear a single message by index from the error_messages array
// index is one base so that 0 can be used as a null value
void clear_message(unsigned char index) {
    if (!index) return;
    index--;
    if (error_messages[index].text) {
        free(error_messages[index].text);
        error_messages[index].text = NULL;
    }
}

// Clear all messages from the error_messages array
void clear_all_messages() {
    for (int i = 0; i < 256; i++) {
        clear_message(i);
    }
}

// Walker callback for highlight_syntax() to process the toksns and set the syntax highlighting

// User date typedef
typedef struct {
    TextBuffer *buffer;
    CodeBuffer *codeBuffer;
    CB_ParseTree *tokenBuffer;
} HighlightSyntaxUserData;

void highlight_syntax_node(CB_Node *node, __attribute__((unused)) size_t depth, void *user_data) {
    HighlightSyntaxUserData *data = (HighlightSyntaxUserData *)user_data;
    if (data == NULL || node == NULL) {
        fprintf(stderr, "PANIC: Invalid data for highlight_syntax_node\n");
        exit(1);
    }
    if (node->child != NULL) {
        return;
    }

    // Get the line and column position of the token
    size_t line = 0, col = 0;
    get_code_buffer_part(data->codeBuffer, node->pos, node->length, &line, &col, NULL);

    // Set the syntax highlighting for the token
    unsigned char highlight = token_to_highlight(node);

    // Set the message number for the token
    unsigned char message_number = 0;
    if (node->message) {
        message_number = add_message(node->message, node->severity);
    }

    // How much of the node has been written to the line - tokens might span lines, so we need to check
    int written = 0;
    // Loop through the lines and set the syntax highlighting and message number
    while (written < node->length) {
        // Get the max node length that fits on the line
        int max_length = line > data->buffer->num_rows?0:(int)strlen(data->buffer->rows[line - 1]) - (int)col; // Ints so that negative values are possible
        int length = (int)node->length - written;
        if (length > max_length) {
            length = max_length;
            written++; // To take into account the newline character
        }
        for (int i = 0; i < length; i++) {
            // Set the syntax highlighting
            data->buffer->row_syntax[line - 1][col + i] = highlight;
            // Set the message number
            data->buffer->message_number[line - 1][col + i] = message_number;
        }
        written += length;
        if (written < node->length) {
            line++;
            col = 0;
        }
    }
}

// Highlights the whole buffer
void highlight_syntax(TextBuffer *buffer) {
    char source_code[100000];
    source_code[0] = '\0';
    // Create one buffer out of all the lines from buffer
    for (int i = 0; i < buffer->num_rows; i++) {
        strcat(source_code, buffer->rows[i]);
        strcat(source_code, "\n");
    }
    CB_ParseTree *tb;
    CodeBuffer *cb;
    highlight_init(source_code, &cb, &tb);

    // Clear all syntax highlighting
    for (int i = 0; i < buffer->num_rows; i++) {
        memset(buffer->row_syntax[i], PAIR_BODY, strlen(buffer->rows[i]));
        memset(buffer->message_number[i], 0, strlen(buffer->rows[i]));
    }
    // Clear all messages
    clear_all_messages();

    /* Walk through the token buffer and set the syntax highlighting */
    HighlightSyntaxUserData data = {buffer, cb, tb};
    cb_walk_tree_top_down(tb, highlight_syntax_node, &data);
}

void die(const char *s) {
    endwin();
    perror(s);
    exit(EXIT_FAILURE);
}

void load_file(TextBuffer *buffer, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        buffer->num_rows = 1;
        buffer->rows = malloc(sizeof(char*));
        buffer->rows[0] = malloc(1);
        buffer->rows[0][0] = '\0';
        buffer->row_syntax = malloc(sizeof(char*));
        buffer->row_syntax[0] = malloc(1);
        buffer->row_syntax[0][0] = '\0';
        buffer->message_number = malloc(sizeof(unsigned char*));
        buffer->message_number[0] = malloc(1);
        buffer->message_number[0][0] = '\0';
        return;
    }

    char line[MAX_LINE_LENGTH];
    buffer->num_rows = 0;
    buffer->rows = NULL;

    while (fgets(line, sizeof(line), fp)) {
        buffer->rows = realloc(buffer->rows, sizeof(char*) * (buffer->num_rows + 1));
        line[strcspn(line, "\n")] = '\0';  // Remove newline character
        buffer->rows[buffer->num_rows] = strdup(line);

        // Syntax highlighting - all characters are PAIR_BODY by default
        buffer->row_syntax = realloc(buffer->row_syntax, sizeof(char*) * (buffer->num_rows + 1));
        buffer->row_syntax[buffer->num_rows] = malloc(strlen(line) + 1);
        for (int i = 0; i < strlen(line); i++) {
            buffer->row_syntax[buffer->num_rows][i] = PAIR_BODY;
        }
        buffer->row_syntax[buffer->num_rows][strlen(line)] = '\0';

        // Message highlighting - all characters are 0 by default
        buffer->message_number = realloc(buffer->message_number, sizeof(unsigned char*) * (buffer->num_rows + 1));
        buffer->message_number[buffer->num_rows] = malloc(strlen(line) + 1);
        for (int i = 0; i < strlen(line); i++) {
            buffer->message_number[buffer->num_rows][i] = '\0';
        }
        buffer->message_number[buffer->num_rows][strlen(line)] = '\0';

        buffer->num_rows++;
    }

    fclose(fp);

    strcpy(loaded_filename, filename);

    highlight_syntax(buffer);
}

void save_file(TextBuffer *buffer, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) die("fopen");

    for (int i = 0; i < buffer->num_rows; i++) {
        fprintf(fp, "%s\n", buffer->rows[i]);
    }

    fclose(fp);
}

void free_buffer(TextBuffer *buffer) {
    for (int i = 0; i < buffer->num_rows; i++) {
        free(buffer->rows[i]);
        free(buffer->row_syntax[i]);
        free(buffer->message_number[i]);
    }
    free(buffer->rows);
    free(buffer->row_syntax);
    free(buffer->message_number);
    clear_all_messages();
}

void editor_refresh(TextBuffer *buffer, int cursor_x, int cursor_y) {
    char line[MAX_LINE_LENGTH];
    char syntax[MAX_LINE_LENGTH];
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Scroll position if necessary - line
    if (cursor_y < scroll_line) {
        scroll_line = cursor_y;
    } else if (cursor_y >= scroll_line + max_y - 3) {
        scroll_line = cursor_y - max_y + 3;
    }

    // Scroll position if necessary - column
    if (cursor_x < scroll_col) {
        scroll_col = cursor_x;
    } else if (cursor_x >= scroll_col + max_x) {
        scroll_col = cursor_x - max_x + 1;
    }

    // Header
    attron(COLOR_PAIR(PAIR_HEADER));
    mvprintw(0, 0, HEADER_TEXT, loaded_filename);
    // Make the rest of the header the same colour
    for (int i = (int)strlen(loaded_filename); i < max_x; i++) {
        addch(' ');
    }

    // Body
    attron(COLOR_PAIR(PAIR_BODY));
    for (int i = 0; i < buffer->num_rows && i < max_y - 3; i++) {
        // copy the buffer to the line taking into account the scroll position
        memcpy(line, buffer->rows[i + scroll_line] + scroll_col, max_x);
        line[max_x] = '\0';
        memcpy(syntax, (char*)buffer->row_syntax[i + scroll_line] + scroll_col, max_x);
        syntax[max_x] = '\0';
        // Position cursor at the beginning of the line
        move(i + 1, 0);
        // print the line with syntax highlighting
        for (int j = 0; j < max_x; j++) {
            if (line[j] == '\0') break;
            int highlight = (int)syntax[j];
            int colour = highlight & 0x0f;
            int underline = highlight & ATTR_UNDERLINE;
            int bold = highlight & ATTR_BOLD;
            int italic = highlight & ATTR_ITALIC;
            int dim = highlight & ATTR_DIM;
            if (!colour) colour = PAIR_BODY;
            int attr = COLOR_PAIR(colour);
            if (underline) attr |= A_UNDERLINE;
            if (bold) attr |= A_BOLD;
            if (italic) attr |= A_ITALIC;
            if (dim) attr |= A_DIM;
            attron(attr);
            addch(line[j]);
            attroff(attr);
        }
        // Clear the rest of the line
        attron(COLOR_PAIR(PAIR_BODY));
        for (int j = (int)strlen(line); j < max_x; j++) {
            addch(' ');
        }

    }

    // Get the message number where the cursor is currently
    unsigned char message_number = buffer->message_number[cursor_y + scroll_line][cursor_x + scroll_col];

    // Footer
    attron(COLOR_PAIR(PAIR_FOOTER));
    // Make the whole of the header the same colour
    // Move to the last line
    move(max_y - 1, 0);
    for (int i = 0; i < max_x; i++) {
        addch(' ');
    }
    mvprintw(max_y - 1, 0, "%s  ", FOOTER_TEXT);
    if (message_number) {
        int severity = error_messages[message_number - 1].severity;
        if (severity == SEVERITY_INFO) {
            attron(COLOR_PAIR(PAIR_INFOMESSAGE));
        } else if (severity == SEVERITY_WARNING) {
            attron(COLOR_PAIR(PAIR_WARNINGMESSAGE));
        } else {
            attron(COLOR_PAIR(PAIR_ERRORMESSAGE));
        }
        printw("[%s]", error_messages[message_number - 1].text);
    }

    // Cursor
    move(cursor_y + 1 - scroll_line, cursor_x - scroll_col);
    refresh();
}

void insert_char(TextBuffer *buffer, int x, int y, int c) {
    char *row = buffer->rows[y];
    int len = (int)strlen(row);

    if (x > len) x = len;

    row = realloc(row, len + 2);  // +1 for new char, +1 for null terminator
    memmove(&row[x + 1], &row[x], len - x + 1);
    row[x] = c;
    buffer->rows[y] = row;

    // Syntax highlighting - all characters are the same as the previous character by default
    unsigned char *syntax = buffer->row_syntax[y];
    syntax = realloc(syntax, len + 2);
    memmove(&syntax[x + 1], &syntax[x], len - x + 1);
    syntax[x] = syntax[x - 1];
    buffer->row_syntax[y] = syntax;

    // Message highlighting - all characters are the same as the previous character by default
    unsigned char *message_number = buffer->message_number[y];
    message_number = realloc(message_number, len + 2);
    memmove(&message_number[x + 1], &message_number[x], len - x + 1);
    message_number[x] = message_number[x - 1];
    buffer->message_number[y] = message_number;
}

void delete_char(TextBuffer *buffer, int x, int y) {
    char *row = buffer->rows[y];
    int len = (int)strlen(row);

    if (x <= 0 || x > len) return;

    memmove(&row[x - 1], &row[x], len - x + 1);
    memmove(&buffer->row_syntax[y][x - 1], &buffer->row_syntax[y][x], len - x + 1);

    // Decide if the message should be deleted - if the message number is different from the previous and the next
    // character's message number, then delete it
    unsigned char message_number = buffer->message_number[y][x - 1];
    unsigned char prev_message_number = (x > 1) ? buffer->message_number[y][x - 2] : 0;
    unsigned char next_message_number = (x < len) ? buffer->message_number[y][x] : 0;
    if (message_number != prev_message_number && message_number != next_message_number) {
        clear_message(message_number);
    }
    // Finally, remove the deleted character from the message number array
    memmove(&buffer->message_number[y][x - 1], &buffer->message_number[y][x], len - x + 1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initialize error messages
    memset(error_messages, 0, sizeof(error_messages));

    TextBuffer buffer = {0, NULL};
    load_file(&buffer, argv[1]);

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    // Start colors
    start_color();
    // Initialize color pairs
    init_pair(PAIR_HEADER, COLOR_WHITE, COLOR_BLUE);
    init_pair(PAIR_FOOTER, COLOR_WHITE, COLOR_BLUE);
    init_pair(PAIR_BODY, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_COMMENT, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_KEYWORD, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_STRING, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_NUM, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_OPERATOR, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(PAIR_VARIABLE, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_ERROR, COLOR_WHITE, COLOR_RED);
    init_pair(PAIR_INFOMESSAGE, COLOR_WHITE, COLOR_GREEN);
    init_pair(PAIR_WARNINGMESSAGE, COLOR_WHITE, COLOR_YELLOW);
    init_pair(PAIR_ERRORMESSAGE, COLOR_WHITE, COLOR_RED);

    int cursor_x = 0;
    int cursor_y = 0;

    while (1) {
        editor_refresh(&buffer, cursor_x, cursor_y);
        int c = getch();

        if (c == CTRL_KEY('q')) {
            break;
        } else if (c == CTRL_KEY('s')) {
            save_file(&buffer, argv[1]);
            mvprintw(LINES - 1, 0, "File saved. Press any key to continue.");
            // Make the rest of the line the same colour
            for (int i = 11; i < COLS; i++) {
                addch(' ');
            }
            getch();
        } else if (c == KEY_UP) {
            if (cursor_y > 0) cursor_y--;
            if (cursor_x > strlen(buffer.rows[cursor_y])) {
                cursor_x = strlen(buffer.rows[cursor_y]);
            }
        } else if (c == KEY_DOWN) {
            if (cursor_y < buffer.num_rows - 1) cursor_y++;
            if (cursor_x > strlen(buffer.rows[cursor_y])) {
                cursor_x = strlen(buffer.rows[cursor_y]);
            }
        } else if (c == KEY_LEFT) {
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = strlen(buffer.rows[cursor_y]);
            }
        } else if (c == KEY_RIGHT) {
            if (cursor_x < strlen(buffer.rows[cursor_y])) {
                cursor_x++;
            } else if (cursor_y < buffer.num_rows - 1) {
                cursor_y++;
                cursor_x = 0;
            }
        } else if (c == KEY_BACKSPACE || c == 127) {
            if (cursor_x > 0) {
                delete_char(&buffer, cursor_x, cursor_y);
                cursor_x--;
            } else if (cursor_y > 0) {
                size_t prev_len = strlen(buffer.rows[cursor_y - 1]);
                size_t next_line_len = strlen(buffer.rows[cursor_y]);
                buffer.rows[cursor_y - 1] = realloc(buffer.rows[cursor_y - 1], prev_len + next_line_len + 1);
                strcat(buffer.rows[cursor_y - 1], buffer.rows[cursor_y]);
                free(buffer.rows[cursor_y]);
                memmove(&buffer.rows[cursor_y], &buffer.rows[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));

                // Syntax highlighting
                buffer.row_syntax[cursor_y - 1] = realloc(buffer.row_syntax[cursor_y - 1], prev_len + next_line_len + 1);
                strcat((char*)buffer.row_syntax[cursor_y - 1], (char*)buffer.row_syntax[cursor_y]);
                free(buffer.row_syntax[cursor_y]);
                memmove(&buffer.row_syntax[cursor_y], &buffer.row_syntax[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));

                // Message highlighting
                buffer.message_number[cursor_y - 1] = realloc(buffer.message_number[cursor_y - 1], prev_len + next_line_len + 1);
                memcpy(buffer.message_number[cursor_y - 1] + prev_len, buffer.message_number[cursor_y], next_line_len + 1);
                free(buffer.message_number[cursor_y]);
                memmove(&buffer.message_number[cursor_y], &buffer.message_number[cursor_y + 1], sizeof(unsigned char*) * (buffer.num_rows - cursor_y - 1));

                buffer.num_rows--;
                cursor_y--;
                cursor_x = prev_len;
            }
            highlight_syntax(&buffer);
        } else if (c == '\n') {
            char *current_row = buffer.rows[cursor_y];
            int len = (int)strlen(current_row);

            char *new_row = strdup(&current_row[cursor_x]);
            current_row[cursor_x] = '\0';
            current_row = realloc(current_row, cursor_x + 1);
            buffer.rows[cursor_y] = current_row;

            buffer.rows = realloc(buffer.rows, sizeof(char*) * (buffer.num_rows + 1));
            memmove(&buffer.rows[cursor_y + 2], &buffer.rows[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));
            buffer.rows[cursor_y + 1] = new_row;

            // Syntax highlighting
            unsigned char *current_syntax = buffer.row_syntax[cursor_y];
            unsigned char *new_syntax = (unsigned char *)strdup((char*)(&current_syntax[cursor_x]));
            current_syntax[cursor_x] = '\0';
            current_syntax = realloc(current_syntax, cursor_x + 1);
            buffer.row_syntax[cursor_y] = current_syntax;

            buffer.row_syntax = realloc(buffer.row_syntax, sizeof(char*) * (buffer.num_rows + 1));
            memmove(&buffer.row_syntax[cursor_y + 2], &buffer.row_syntax[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));
            buffer.row_syntax[cursor_y + 1] = new_syntax;

            // Message highlighting
            unsigned char *current_message_number = buffer.message_number[cursor_y];
            unsigned char *new_message_number = malloc(len - cursor_x + 1);
            memcpy(new_message_number, &current_message_number[cursor_x], len - cursor_x + 1);
            memset(&current_message_number[cursor_x], 0, len - cursor_x);
            current_message_number = realloc(current_message_number, cursor_x + 1);
            buffer.message_number[cursor_y] = current_message_number;

            buffer.message_number = realloc(buffer.message_number, sizeof(unsigned char*) * (buffer.num_rows + 1));
            memmove(&buffer.message_number[cursor_y + 2], &buffer.message_number[cursor_y + 1], sizeof(unsigned char*) * (buffer.num_rows - cursor_y - 1));
            buffer.message_number[cursor_y + 1] = new_message_number;

            buffer.num_rows++;
            cursor_y++;
            cursor_x = 0;
            highlight_syntax(&buffer);
        } else if (isprint(c)) {
            insert_char(&buffer, cursor_x, cursor_y, c);
            cursor_x++;
            highlight_syntax(&buffer);
        }
    }

    endwin();
    free_buffer(&buffer);
    return 0;
}
