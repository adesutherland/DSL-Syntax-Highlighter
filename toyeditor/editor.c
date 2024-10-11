//
// Created by Adrian Sutherland on 11/10/2024.
//
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

// For the file name
char loaded_filename[256];

// Scroll position - line and column
int scroll_line = 0;
int scroll_col = 0;

typedef struct {
    int num_rows;
    char **rows;
    char **row_syntax; // Syntax highlighting - contains the type of each character corresponding to PAIR_* colours
} TextBuffer;

// Simple Dummy Highlighter - handles has comments, whitespace, numbers and symbols
// Highligts the whole buffer
void highlight_syntax(TextBuffer *buffer) {
    for (int i = 0; i < buffer->num_rows; i++) {
        char *row = buffer->rows[i];
        char *syntax = buffer->row_syntax[i];
        for (int j = 0; j < strlen(row); j++) {
            char c = row[j];
            if (c == '#') {
                // Make the rest of the line a comment
                for (int k = j; k < strlen(row); k++) {
                    syntax[k] = PAIR_COMMENT;
                }
                break;

            } else if (isspace(c)) {
                syntax[j] = PAIR_BODY;

            } else if (isdigit(c)) {
                syntax[j] = PAIR_NUM;

            } else if (isalpha(c) || c == '_') {
                syntax[j] = PAIR_VARIABLE;

            } else {
                syntax[j] = PAIR_OPERATOR;
            }
        }
    }
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
    }
    free(buffer->rows);
    free(buffer->row_syntax);
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
        strncpy(line, buffer->rows[i + scroll_line] + scroll_col, max_x);
        line[max_x] = '\0';
        strncpy(syntax, buffer->row_syntax[i + scroll_line] + scroll_col, max_x);
        syntax[max_x] = '\0';
        // Position cursor at the beginning of the line
        move(i + 1, 0);
        // print the line with syntax highlighting
        for (int j = 0; j < max_x; j++) {
            if (line[j] == '\0') break;
            int colour = (int)syntax[j];
            if (!colour) colour = PAIR_BODY;
            attron(COLOR_PAIR(colour));
            addch(line[j]);
        }
        // Clear the rest of the line
        attron(COLOR_PAIR(PAIR_BODY));
        for (int j = strlen(line); j < max_x; j++) {
            addch(' ');
        }

    }

    // Footer
    attron(COLOR_PAIR(PAIR_FOOTER));
    mvprintw(max_y - 1, 0, "%s", FOOTER_TEXT);
    // Make the rest of the header the same colour
    for (int i = (int)strlen(FOOTER_TEXT); i < max_x; i++) {
        addch(' ');
    }

    move(cursor_y + 1 - scroll_line, cursor_x - scroll_col);
    refresh();
}

void insert_char(TextBuffer *buffer, int x, int y, int c) {
    char *row = buffer->rows[y];
    int len = strlen(row);

    if (x > len) x = len;

    row = realloc(row, len + 2);  // +1 for new char, +1 for null terminator
    memmove(&row[x + 1], &row[x], len - x + 1);
    row[x] = c;
    buffer->rows[y] = row;

    // Syntax highlighting - all characters are the same as the previous character by default
    char *syntax = buffer->row_syntax[y];
    syntax = realloc(syntax, len + 2);
    memmove(&syntax[x + 1], &syntax[x], len - x + 1);
    syntax[x] = syntax[x - 1];
    buffer->row_syntax[y] = syntax;
}

void delete_char(TextBuffer *buffer, int x, int y) {
    char *row = buffer->rows[y];
    int len = strlen(row);

    if (x <= 0 || x > len) return;

    memmove(&row[x - 1], &row[x], len - x + 1);
    memmove(&buffer->row_syntax[y][x - 1], &buffer->row_syntax[y][x], len - x + 1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

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
    init_pair(PAIR_COMMENT, COLOR_BLUE, COLOR_BLACK);
    init_pair(PAIR_KEYWORD, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_STRING, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_NUM, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(PAIR_OPERATOR, COLOR_RED, COLOR_BLACK);
    init_pair(PAIR_VARIABLE, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_ERROR, COLOR_WHITE, COLOR_RED);

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
                int prev_len = strlen(buffer.rows[cursor_y - 1]);
                buffer.rows[cursor_y - 1] = realloc(buffer.rows[cursor_y - 1], prev_len + strlen(buffer.rows[cursor_y]) + 1);
                strcat(buffer.rows[cursor_y - 1], buffer.rows[cursor_y]);
                free(buffer.rows[cursor_y]);
                memmove(&buffer.rows[cursor_y], &buffer.rows[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));

                // Syntax highlighting
                buffer.row_syntax[cursor_y - 1] = realloc(buffer.row_syntax[cursor_y - 1], prev_len + strlen(buffer.row_syntax[cursor_y]) + 1);
                strcat(buffer.row_syntax[cursor_y - 1], buffer.row_syntax[cursor_y]);
                free(buffer.row_syntax[cursor_y]);
                memmove(&buffer.row_syntax[cursor_y], &buffer.row_syntax[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));

                buffer.num_rows--;
                cursor_y--;
                cursor_x = prev_len;
            }
            highlight_syntax(&buffer);
        } else if (c == '\n') {
            char *current_row = buffer.rows[cursor_y];
            int len = strlen(current_row);

            char *new_row = strdup(&current_row[cursor_x]);
            current_row[cursor_x] = '\0';
            current_row = realloc(current_row, cursor_x + 1);
            buffer.rows[cursor_y] = current_row;

            buffer.rows = realloc(buffer.rows, sizeof(char*) * (buffer.num_rows + 1));
            memmove(&buffer.rows[cursor_y + 2], &buffer.rows[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));
            buffer.rows[cursor_y + 1] = new_row;

            // Syntax highlighting
            char *current_syntax = buffer.row_syntax[cursor_y];
            char *new_syntax = strdup(&current_syntax[cursor_x]);
            current_syntax[cursor_x] = '\0';
            current_syntax = realloc(current_syntax, cursor_x + 1);
            buffer.row_syntax[cursor_y] = current_syntax;

            buffer.row_syntax = realloc(buffer.row_syntax, sizeof(char*) * (buffer.num_rows + 1));
            memmove(&buffer.row_syntax[cursor_y + 2], &buffer.row_syntax[cursor_y + 1], sizeof(char*) * (buffer.num_rows - cursor_y - 1));
            buffer.row_syntax[cursor_y + 1] = new_syntax;

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

