//
// Created by Adrian Sutherland on 11/10/2024.
//
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
typedef HANDLE ThreadType;
typedef HANDLE MutexType; /* This is for the global critical section mutex */
/* Define ThreadFunctionType to match Windows CreateThread */
typedef DWORD (WINAPI *ThreadFunctionType)(LPVOID lpThreadParameter);
#else /* POSIX (Linux, macOS) */
#include <unistd.h>
#endif

#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"
#include "serialization.h"
#include "dslsyntax_log.h"
#include "parser_highlighter.h" // Toy parser highlighter header

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
    CodeBuffer *code_buffer; // CodeBuffer for the syntax highlighting
} TextBuffer;


// SDL Highlighter End Point
CommunicationFunctions *sdlhighlighter = NULL;

// For the file name
char loaded_filename[256];

// Screen Size
int max_y = 0;
int max_x = 0;

// Scroll position - line and column
int scroll_line = 0;
int scroll_col = 0;

// Function to handle errors and exit the program
void die(const char *s) {
    endwin();
    perror(s);
    exit(EXIT_FAILURE);
}

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
int getch_or_parse_event(TextBuffer *buffer) {
    int result_char = ERR;
    WINDOW *win = stdscr;

    // Send Any Updates to the SDL Highlighter
    if (buffer->code_buffer->transaction_count > 0) {
        if (!editor_is_parsing_thread_active()) {
            process_delta(buffer->code_buffer);
        }
    }

    if (!editor_is_parsing_thread_active()) {
        // Optimized path: No active event source, just call wgetch directly ...
        // However, we do need to check if an unhandled parse thread just exited
        if (check_parse_complete_event() == 1) {
            reset_parse_complete_event();
            return GETCH_EVENT_RAISED;
        }
        return wgetch(win);
    }

    // This gives access to any parsing thread
    if (exit_codeblock_critical_section() != 0) {
        fprintf(stderr, "polled_getch: Failed to exit CS!\n");
        die("polled_getch: Critical section error");
    }

    /* Polling path: Event source is active, perform polling. */
    for (;;) { /* Polling loop */
        /* Check for the event */
        if (check_parse_complete_event() == 1) {
            reset_parse_complete_event();
            result_char = GETCH_EVENT_RAISED;
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

    // This takes control for this editor main thread
    if (enter_codeblock_critical_section() != 0) {
        fprintf(stderr, "polled_getch: CRITICAL - Failed to re-enter CS!\n");
        die("polled_getch: Critical section error");
    }

    return result_char;
}

// Convert the CB_ParseTree to a highlight code
// 4 least significant bits are used for the pair code
// 5th bit used to indicate whether the token is underlined (16)
// 6th bit used to indicate whether the token is bold (32)
// 7th bit used to indicate whether the token is italicized (64)
// 8th bit used to indicate whether the token is dimmed (128)
static int highlight_log_count = 0;

static unsigned char cb_nodetype_to_highlight(CodeBufferCharacter attribute) {
    unsigned char highlight;
    if (highlight_log_count < 50 && attribute.token_type != 0) {
        LOG("highlight: type=%d, severity=%d", (int)attribute.token_type, (int)attribute.severity);
        highlight_log_count++;
    }
    switch (attribute.severity) {
        case CB_ERROR:
            highlight = PAIR_ERRORMESSAGE;
            break;
        case CB_WARNING:
            highlight = PAIR_WARNINGMESSAGE;
            break;
        case CB_INFORMATION:
            highlight = PAIR_INFOMESSAGE;
            break;
        default:
            switch (attribute.token_type) {
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
            case LEXER_TOKEN:
            case LEXER_WHITESPACE:
            case LEXER_EOF:
                    highlight = PAIR_BODY;
                    break;
            case SYNTAX_ERROR:
                    highlight = PAIR_ERROR;
                    break;
            default:
                    highlight = PAIR_BODY;
            }
    }

    return highlight;
}

// Function to create the source code char* from the TextBuffer rows
// Returns a newly allocated string that contains the source code
char *create_source_code_from_buffer(TextBuffer *buffer) {
    size_t total_length = 0;
    for (int i = 0; i < buffer->num_rows; i++) {
        total_length += strlen(buffer->rows[i]) + 1; // +1 for newline
    }
    char *source_code = malloc(total_length + 1);
    if (!source_code) die("malloc"); // die() Never returns

    source_code[0] = '\0';
    for (int i = 0; i < buffer->num_rows; i++) {
        strcat(source_code, buffer->rows[i]);
        strcat(source_code, "\n");
    }
    return source_code;
}

void load_file(TextBuffer *buffer, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        buffer->num_rows = 1;
        buffer->rows = malloc(sizeof(char*));
        buffer->rows[0] = malloc(1);
        buffer->rows[0][0] = '\0';
        return;
    }

    char line[MAX_LINE_LENGTH];
    buffer->num_rows = 0;
    buffer->rows = NULL;

    while (fgets(line, sizeof(line), fp)) {
        buffer->rows = realloc(buffer->rows, sizeof(char*) * (buffer->num_rows + 1));
        line[strcspn(line, "\n")] = '\0';  // Remove newline character
        buffer->rows[buffer->num_rows] = strdup(line);

        buffer->num_rows++;
    }

    fclose(fp);

    strcpy(loaded_filename, filename);

    // Create editor CodeBuffer and load the file into it
    char* source_code = create_source_code_from_buffer(buffer);
    buffer->code_buffer = create_code_buffer(sdlhighlighter,0);
    InitialLoad *initial = create_initial_load(loaded_filename, source_code);
    load_initial_content(buffer->code_buffer, initial);
    free(source_code); // Free the source code string
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
    }
    free(buffer->rows);
}

void editor_refresh(TextBuffer *buffer, int cursor_x, int cursor_y) {
    highlight_log_count = 0;
    //clear();
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
    int i, j;
    attron(COLOR_PAIR(PAIR_BODY));
    for (i = 0; i + scroll_line < buffer->num_rows && i < max_y - 2; i++) {
        // Position cursor at the beginning of the line
        move(i + 1, 0);
        // print the line with syntax highlighting
        for (j = 0; j < max_x; j++) {
            char ch = buffer->rows[i + scroll_line][j + scroll_col];
            if (ch == '\0') break;
            
            int highlight = PAIR_BODY;
            if (buffer->code_buffer && buffer->code_buffer->lines && (i + scroll_line) < buffer->code_buffer->line_count) {
                if (j + scroll_col < buffer->code_buffer->lines[i + scroll_line].length) {
                    highlight = cb_nodetype_to_highlight(buffer->code_buffer->lines[i + scroll_line].characters[j + scroll_col]);
                }
            }
            
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
            
            wattrset(stdscr, attr);
            addch(ch);
        }
        // Reset Attributes
        wattrset(stdscr, COLOR_PAIR(PAIR_BODY));

        // Fill the rest of the line with spaces
        for (; j < max_x; j++) {
            addch(' ');
        }
    }

    // Fill the rest of the body with spaces
    for (; i < max_y - 2; i++) {
        move(i + 1, 0);
        for (j = 0; j < max_x; j++) {
            addch(' ');
        }
    }

    // Get the message where the cursor is currently
    char severity = CB_NONE;
    char* message = 0;
    if (buffer->code_buffer && buffer->code_buffer->lines && cursor_y < buffer->code_buffer->line_count) {
        if (cursor_x < buffer->code_buffer->lines[cursor_y].length) {
            severity = buffer->code_buffer->lines[cursor_y].characters[cursor_x].severity;
            CB_Node *node = buffer->code_buffer->lines[cursor_y].characters[cursor_x].node;
            if (node) {
                message = node->message;
            }
        }
    }

    // Footer
    attron(COLOR_PAIR(PAIR_FOOTER));
    // Make the whole of the footer the same colour
    // Move to the last line
    move(max_y - 1, 0);
    for (j = 0; j < max_x; j++) {
        addch(' ');
    }
    mvprintw(max_y - 1, 0, "%s  ", FOOTER_TEXT);
    if (message) {
        switch (severity) {
            case CB_ERROR:
                attron(COLOR_PAIR(PAIR_ERRORMESSAGE));
                break;
            case CB_WARNING:
                attron(COLOR_PAIR(PAIR_WARNINGMESSAGE));
                break;
            default:
                attron(COLOR_PAIR(PAIR_INFOMESSAGE));
        }
        // Calculate the max message length om the screen
        int message_length = (int)strlen(message);
        if (message_length > max_x - 2) {
            message_length = max_x - 2; // Leave space for the brackets
        }
        // Print the message
        addch('[');
        for (i = 0; i < message_length; i++) {
            addch(message[i]);
        }
        addch(']');
        // How many characters are left on the line?
        int remaining_length = max_x - 2 - message_length;
        if (remaining_length) {
            // Fill the rest of the line with spaces
            attron(COLOR_PAIR(PAIR_FOOTER));
            for (i = 0; i < remaining_length; i++) {
                addch(' ');
            }
        }
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

    /* Apply DSL Highlighter Transaction */
    CodeBuffer *cb = buffer->code_buffer;

    // Create a transaction to apply
    Transaction txn;
    txn.type = TRANSACTION_ADDCHARS;
    txn.pos_line = y; // Zero-based index
    txn.pos_col = x;
    txn.count = 1; // Insert one character
    char content[2] = {0}; // Buffer for the character to insert
    content[0] = (char)c; // Convert the character to a string
    txn.content = content;

    editor_apply_transaction(cb, txn);
}

void delete_char(TextBuffer *buffer, int x, int y) {
    char *row = buffer->rows[y];
    int len = (int)strlen(row);

    if (x <= 0 || x > len) return;

    memmove(&row[x - 1], &row[x], len - x + 1);

    /* Apply DSL Highlighter Transaction */
    CodeBuffer *cb = buffer->code_buffer;
    // Create a transaction to apply
    Transaction txn;
    txn.type = TRANSACTION_DELETECHARS;
    txn.pos_line = y; // Zero-based index
    txn.pos_col = x - 1; // Zero-based index
    txn.count = 1; // Delete one character
    txn.content = NULL; // No content for delete transaction
    editor_apply_transaction(cb, txn);
}

void join_line(TextBuffer *buffer, int y) {
    if (y <= 0 || y >= buffer->num_rows) return; // Invalid line number

    char *prev_row = buffer->rows[y - 1];
    char *next_row = buffer->rows[y];

    size_t prev_len = strlen(prev_row);
    size_t next_len = strlen(next_row);

    // Reallocate the previous row to hold the next row's content
    prev_row = realloc(prev_row, prev_len + next_len + 1); // +1 for null terminator
    if (!prev_row) die("realloc");

    // Append the next row to the previous row
    memcpy(&prev_row[prev_len], next_row, next_len + 1); // +1 for null terminator
    buffer->rows[y - 1] = prev_row;

    // Free the next row and remove it from the array
    free(next_row);
    memmove(&buffer->rows[y], &buffer->rows[y + 1], sizeof(char*) * (buffer->num_rows - y - 1));

    buffer->num_rows--;

    /* Apply DSL Highlighter Transaction */
    CodeBuffer *cb = buffer->code_buffer;
    Transaction txn;
    txn.type = TRANSACTION_JOINLINES;
    txn.pos_line = y - 1; // Zero-based index
    txn.pos_col = (int)prev_len; // Position after the last character of the previous line
    txn.count = 1; // Join one line
    txn.content = NULL; // No content for join transaction
    editor_apply_transaction(cb, txn);
}

void split_line(TextBuffer *buffer, int x, int y) {
    if (y < 0 || y >= buffer->num_rows) return; // Invalid line number
    char *row = buffer->rows[y];
    size_t len = strlen(row);
    if (x < 0 || x >= len) return; // Invalid column

    // Create a new row for the split
    char *new_row = strdup(&row[x]);
    if (!new_row) die("strdup");

    // Truncate the original row
    row[x] = '\0';
    buffer->rows[y] = realloc(row, x + 1); // +1 for null terminator

    // Insert the new row into the buffer
    buffer->rows = realloc(buffer->rows, sizeof(char*) * (buffer->num_rows + 1));
    memmove(&buffer->rows[y + 2], &buffer->rows[y + 1], sizeof(char*) * (buffer->num_rows - y - 1));
    buffer->rows[y + 1] = new_row;

    buffer->num_rows++;

    /* Apply DSL Highlighter Transaction */
    CodeBuffer *cb = buffer->code_buffer;
    Transaction txn;
    txn.type = TRANSACTION_SPLITLINE;
    txn.pos_line = y; // Zero-based index
    txn.pos_col = x; // Position to split at
    txn.count = 1; // Split one line
    txn.content = NULL; // No content for split transaction
    editor_apply_transaction(cb, txn);
}

void add_line(TextBuffer *buffer, int y) {
    if (y < 0 || y > buffer->num_rows) return; // Invalid line number

    // Allocate memory for the new row
    char *new_row = malloc(1);
    if (!new_row) die("malloc");
    new_row[0] = '\0'; // Initialize to empty string

    // Insert the new row into the buffer
    buffer->rows = realloc(buffer->rows, sizeof(char*) * (buffer->num_rows + 1));
    memmove(&buffer->rows[y + 1], &buffer->rows[y], sizeof(char*) * (buffer->num_rows - y));
    buffer->rows[y] = new_row;

    buffer->num_rows++;

    /* Apply DSL Highlighter Transaction */
    CodeBuffer *cb = buffer->code_buffer;
    Transaction txn;
    txn.type = TRANSACTION_ADDLINE;
    txn.pos_line = y; // Zero-based index
    txn.pos_col = 0; // Position at the start of the new line
    txn.count = 1; // Add one line
    txn.content = NULL; // No content for add line transaction
    editor_apply_transaction(cb, txn);
}
int main(int argc, char *argv[]) {
    char *filename = NULL;
    char *parser_path = "../toyparser/tp"; // Default
    char *parser_args = ""; // Default
    int debug = 0;
    int use_socket = 0;
    int port = 8080;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) debug = 1;
        else if (strcmp(argv[i], "--socket") == 0) use_socket = 1;
        else if (i + 1 < argc && strcmp(argv[i], "--port") == 0) {
            port = atoi(argv[++i]);
            use_socket = 1;
        }
        else if (i + 1 < argc && (strcmp(argv[i], "--parser") == 0 || strcmp(argv[i], "-p") == 0)) {
            parser_path = argv[++i];
        }
        else if (i + 1 < argc && (strcmp(argv[i], "--parser-args") == 0 || strcmp(argv[i], "-a") == 0)) {
            parser_args = argv[++i];
        }
        else filename = argv[i];
    }

    if (!filename) {
        printf("Usage: %s [-d] [--socket] [--port 8080] [--parser ../toyparser/tp] [--parser-args \"args\"] filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (debug) cb_log_init("editor.log");
    LOG("Toy Editor starting for file: %s", filename);

    // Set up the SDL highlighter
    editor_init(); // Initialize the editor side of the library
    
    if (use_socket) {
        LOG("Connecting to parser via socket on port %d", port);
        sdlhighlighter = create_socket_communication_functions("127.0.0.1", port);
    } else {
        LOG("Launching parser via stdio: %s %s", parser_path, parser_args);
        char cmd[2048];
        /* Pass debug flag to parser if editor is in debug mode */
        if (debug) sprintf(cmd, "%s -d %s", parser_path, parser_args);
        else sprintf(cmd, "%s %s", parser_path, parser_args);
        sdlhighlighter = create_stdio_communication_functions(cmd);
    }

    if (!sdlhighlighter) {
        fprintf(stderr, "Failed to initialize highlighter communication\n");
        exit(EXIT_FAILURE);
    }

    // This takes control for this editor main thread
    if (enter_codeblock_critical_section() != 0) {
        fprintf(stderr, "CRITICAL - Failed to re-enter CS!\n");
        die("Critical section error");
    }

    TextBuffer buffer = {0, NULL, NULL};
    load_file(&buffer, filename);

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL); // nable all mouse events

    // Initialize color pairs
    start_color(); // Start colors
    init_pair(PAIR_HEADER, COLOR_WHITE, COLOR_BLUE);
    init_pair(PAIR_FOOTER, COLOR_WHITE, COLOR_BLUE);
    init_pair(PAIR_BODY, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_COMMENT, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_KEYWORD, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_STRING, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_NUM, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_OPERATOR, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(PAIR_VARIABLE, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_ERROR, COLOR_WHITE, COLOR_RED);
    init_pair(PAIR_INFOMESSAGE, COLOR_WHITE, COLOR_GREEN);
    init_pair(PAIR_WARNINGMESSAGE, COLOR_WHITE, COLOR_YELLOW);
    init_pair(PAIR_ERRORMESSAGE, COLOR_WHITE, COLOR_RED);

    int cursor_x = 0;
    int cursor_y = 0;

    while (1) {
        editor_refresh(&buffer, cursor_x, cursor_y);
        int c = getch_or_parse_event(&buffer);

        if (c == GETCH_EVENT_RAISED) {
            // Handle the event raised by the parser
            continue;
        }
        if (c == CTRL_KEY('q')) {
            break;
        }
        if (c == CTRL_KEY('s')) {
            save_file(&buffer, argv[1]);
            mvprintw(LINES - 1, 0, "File saved. Press any key to continue.");
            // Make the rest of the line the same colour
            for (int i = 11; i < COLS; i++) {
                addch(' ');
            }
            getch();
            continue;
        }
        // Handle arrow keys and other special keys
        if (c == KEY_UP) {
            if (cursor_y > 0) {
                cursor_y--;
                if (cursor_x > strlen(buffer.rows[cursor_y])) {
                    cursor_x = (int)strlen(buffer.rows[cursor_y]);
                }
            }
            continue;
        }
        // Page Up
        if (c == KEY_PPAGE) {
            if (cursor_y > 0) {
                int page_size = max_y - 3;
                if (page_size < 1) page_size = 1;
                cursor_y -= page_size;
                if (cursor_y < 0) cursor_y = 0;
                if (cursor_x > strlen(buffer.rows[cursor_y])) {
                    cursor_x = (int)strlen(buffer.rows[cursor_y]);
                }
            }
            continue;
        }
        if (c == KEY_DOWN) {
            if (cursor_y < buffer.num_rows - 1) {
                cursor_y++;
                if (cursor_x > strlen(buffer.rows[cursor_y])) {
                    cursor_x = (int)strlen(buffer.rows[cursor_y]);
                }
            }
            continue;
        }
        // Page Down
        if (c == KEY_NPAGE) {
            if (cursor_y < buffer.num_rows - 1) {
                int page_size = max_y - 3;
                if (page_size < 1) page_size = 1;
                cursor_y += page_size;
                if (cursor_y >= buffer.num_rows) cursor_y = buffer.num_rows - 1;
                if (cursor_y < 0) cursor_y = 0;
                if (cursor_x > strlen(buffer.rows[cursor_y])) {
                    cursor_x = (int)strlen(buffer.rows[cursor_y]);
                }
            }
            continue;
        }
        if (c == KEY_LEFT) {
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = (int)strlen(buffer.rows[cursor_y]);
            }
            continue;
        }
        if (c == KEY_RIGHT) {
            if (cursor_x < strlen(buffer.rows[cursor_y])) {
                cursor_x++;
            } else if (cursor_y < buffer.num_rows - 1) {
                cursor_y++;
                cursor_x = 0;
            }
            continue;
        }
        if (c == KEY_BACKSPACE || c == 127) {
            if (cursor_x > 0) {
                delete_char(&buffer, cursor_x, cursor_y);
                cursor_x--;
            } else if (cursor_y > 0) {
                int new_cursor_x = (int)strlen(buffer.rows[cursor_y - 1]);
                join_line(&buffer, cursor_y);
                // Move cursor
                cursor_y--;
                cursor_x = new_cursor_x;
            }
            continue;
        }
        if (c == '\n') {
            if (cursor_x == strlen(buffer.rows[cursor_y])) {
                add_line(&buffer, cursor_y + 1); // Add a new line after the current line
            }
            else {
                split_line(&buffer, cursor_x, cursor_y);
            }
            cursor_y++;
            cursor_x = 0;
            continue;
        }
        if (isprint(c)) {
            insert_char(&buffer, cursor_x, cursor_y, c);
            cursor_x++;
            continue;
        }
        // Mouse Events
        if (c == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                if (event.bstate & BUTTON1_CLICKED) {
                    // Left click
                    cursor_x = event.x - scroll_col; // Adjust for scroll
                    cursor_y = event.y - 1 + scroll_line; // Adjust for scroll and header
                    if (cursor_x < 0) cursor_x = 0;
                    if (cursor_y < 0) cursor_y = 0;
                    if (cursor_y >= buffer.num_rows) cursor_y = buffer.num_rows - 1;
                    if (cursor_x > (int)strlen(buffer.rows[cursor_y])) {
                        cursor_x = (int)strlen(buffer.rows[cursor_y]);
                    }
                }
            }
            continue;
        }
    }

    // This gives access to any parsing thread
    if (exit_codeblock_critical_section() != 0) {
        fprintf(stderr, "Failed to exit CS!\n");
        die("Critical section error");
    }

    /* Free the editor library - this joins the parser thread if active */
    editor_free();

    /* Free the CodeBuffer */
    free_code_buffer(buffer.code_buffer);
    buffer.code_buffer = 0;

    /* Free the communication functions */
    if (sdlhighlighter) {
        if (use_socket) {
            if (sdlhighlighter->comms_data) free(sdlhighlighter->comms_data);
            free(sdlhighlighter);
        } else {
            free_stdio_communication_functions(sdlhighlighter);
        }
    }

    // Clean up ncurses
    clear(); // Clear the screen before exiting
    endwin();

    free_buffer(&buffer);
    return 0;
}
