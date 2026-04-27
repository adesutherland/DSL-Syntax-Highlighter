#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "cmark-gfm-extension_api.h"
#include "dslsyntax_log.h"
#include "markdown_parser.h"

int markdown_slow_mode = 0;

typedef struct MarkdownSourceMap {
    const char *source;
    size_t byte_length;
    size_t char_count;
    size_t *char_to_byte;
    size_t line_count;
    size_t *line_start_chars;
    size_t *line_start_bytes;
} MarkdownSourceMap;

static int utf8_width(unsigned char ch) {
    if ((ch & 0x80) == 0) return 1;
    if ((ch & 0xE0) == 0xC0) return 2;
    if ((ch & 0xF0) == 0xE0) return 3;
    if ((ch & 0xF8) == 0xF0) return 4;
    return 1;
}

static int append_size(size_t **values, size_t *count, size_t value) {
    size_t *new_values = (size_t*)safe_realloc(*values, (*count + 1) * sizeof(size_t));
    if (!new_values) return -1;
    *values = new_values;
    (*values)[(*count)++] = value;
    return 0;
}

static int append_line_start(MarkdownSourceMap *map, size_t char_pos, size_t byte_pos) {
    size_t *new_chars = (size_t*)safe_realloc(map->line_start_chars, (map->line_count + 1) * sizeof(size_t));
    size_t *new_bytes;
    if (!new_chars) return -1;
    map->line_start_chars = new_chars;

    new_bytes = (size_t*)safe_realloc(map->line_start_bytes, (map->line_count + 1) * sizeof(size_t));
    if (!new_bytes) return -1;
    map->line_start_bytes = new_bytes;

    map->line_start_chars[map->line_count] = char_pos;
    map->line_start_bytes[map->line_count] = byte_pos;
    map->line_count++;
    return 0;
}

static int source_map_init(MarkdownSourceMap *map, const char *source) {
    size_t byte_pos = 0;
    size_t char_pos = 0;

    memset(map, 0, sizeof(*map));
    map->source = source ? source : "";
    map->byte_length = strlen(map->source);

    if (append_line_start(map, 0, 0) != 0) return -1;

    while (byte_pos < map->byte_length) {
        int width;

        if (append_size(&map->char_to_byte, &map->char_count, byte_pos) != 0) return -1;
        width = utf8_width((unsigned char)map->source[byte_pos]);
        if (byte_pos + (size_t)width > map->byte_length) width = 1;

        if (map->source[byte_pos] == '\n') {
            if (append_line_start(map, char_pos + 1, byte_pos + 1) != 0) return -1;
        }

        byte_pos += (size_t)width;
        char_pos++;
    }

    if (append_size(&map->char_to_byte, &map->char_count, map->byte_length) != 0) return -1;
    map->char_count--;
    return 0;
}

static void source_map_free(MarkdownSourceMap *map) {
    free(map->char_to_byte);
    free(map->line_start_chars);
    free(map->line_start_bytes);
}

static size_t byte_to_char(const MarkdownSourceMap *map, size_t byte_pos) {
    size_t lo = 0;
    size_t hi = map->char_count;

    if (byte_pos >= map->byte_length) return map->char_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (map->char_to_byte[mid] < byte_pos) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static size_t line_column_to_char(const MarkdownSourceMap *map, int line, int column, int exclusive) {
    size_t line_index;
    size_t line_byte;
    size_t next_line_byte;
    size_t byte_pos;
    int column_offset;

    if (line <= 0) return 0;
    line_index = (size_t)(line - 1);
    if (line_index >= map->line_count) return map->char_count;

    line_byte = map->line_start_bytes[line_index];
    next_line_byte = (line_index + 1 < map->line_count) ? map->line_start_bytes[line_index + 1] : map->byte_length;
    column_offset = column <= 0 ? 0 : column - 1;
    if (exclusive) column_offset = column <= 0 ? 0 : column;

    byte_pos = line_byte + (size_t)column_offset;
    if (byte_pos > next_line_byte) byte_pos = next_line_byte;
    if (byte_pos > map->byte_length) byte_pos = map->byte_length;
    return byte_to_char(map, byte_pos);
}

static char md_char_at(const MarkdownSourceMap *map, size_t pos) {
    size_t byte_pos;
    if (pos >= map->char_count) return '\0';
    byte_pos = map->char_to_byte[pos];
    if ((unsigned char)map->source[byte_pos] >= 0x80) return '\0';
    return map->source[byte_pos];
}

static int md_match(const MarkdownSourceMap *map, size_t pos, size_t end, const char *text) {
    size_t i = 0;
    while (text[i]) {
        if (pos + i >= end || md_char_at(map, pos + i) != text[i]) return 0;
        i++;
    }
    return 1;
}

static void add_token(CB_ParseTree *tb, CB_NodeType type, size_t pos, size_t length) {
    if (length == 0) return;
    cb_add_child_node(tb, cb_create_node(type, pos, length));
}

static size_t scan_spaces(const MarkdownSourceMap *map, size_t pos, size_t end) {
    while (pos < end) {
        char ch = md_char_at(map, pos);
        if (ch != ' ' && ch != '\t') break;
        pos++;
    }
    return pos;
}

static size_t scan_until(const MarkdownSourceMap *map, size_t pos, size_t end, const char *needle) {
    while (pos < end) {
        if (md_match(map, pos, end, needle)) return pos + strlen(needle);
        pos++;
    }
    return end;
}

static int is_inline_special(char ch) {
    return ch == '<' || ch == '`' || ch == '[' || ch == ']' || ch == '(' || ch == ')' ||
           ch == '!' || ch == '*' || ch == '_' || ch == '~' || ch == '#' || ch == '|' ||
           ch == '\\';
}

static void scan_inline(CB_ParseTree *tb, const MarkdownSourceMap *map, size_t pos, size_t end) {
    while (pos < end) {
        char ch = md_char_at(map, pos);
        size_t start = pos;

        if (ch == ' ' || ch == '\t') {
            pos = scan_spaces(map, pos, end);
            add_token(tb, LEXER_WHITESPACE, start, pos - start);
        } else if (md_match(map, pos, end, "<!--")) {
            pos = scan_until(map, pos + 4, end, "-->");
            add_token(tb, LEXER_COMMENT, start, pos - start);
        } else if (ch == '`') {
            size_t ticks = 0;
            while (pos + ticks < end && md_char_at(map, pos + ticks) == '`') ticks++;
            pos += ticks;
            while (pos < end) {
                size_t matched = 0;
                while (matched < ticks && pos + matched < end && md_char_at(map, pos + matched) == '`') matched++;
                if (matched == ticks) {
                    pos += ticks;
                    break;
                }
                pos++;
            }
            add_token(tb, LEXER_STRING_LITERAL, start, pos - start);
        } else if (ch == '[' || ch == ']' || ch == '(' || ch == ')') {
            add_token(tb, LEXER_SEPARATOR, pos, 1);
            pos++;
        } else if (ch == '!' || ch == '*' || ch == '_' || ch == '~' || ch == '#' || ch == '|' || ch == '\\') {
            while (pos < end && md_char_at(map, pos) == ch) pos++;
            add_token(tb, LEXER_OPERATOR, start, pos - start);
        } else if (isdigit((unsigned char)ch)) {
            while (pos < end && isdigit((unsigned char)md_char_at(map, pos))) pos++;
            add_token(tb, LEXER_NUMBER_LITERAL, start, pos - start);
        } else {
            while (pos < end) {
                ch = md_char_at(map, pos);
                if (ch == ' ' || ch == '\t' || is_inline_special(ch)) break;
                pos++;
            }
            if (pos == start) pos++;
            add_token(tb, LEXER_TOKEN, start, pos - start);
        }
    }
}

static int is_fence_line(const MarkdownSourceMap *map, size_t pos, size_t end, size_t *marker_end, char *fence_char) {
    size_t marker_pos = scan_spaces(map, pos, end);
    char ch = md_char_at(map, marker_pos);
    size_t run = 0;

    if (marker_pos - pos > 3 || (ch != '`' && ch != '~')) return 0;
    while (marker_pos + run < end && md_char_at(map, marker_pos + run) == ch) run++;
    if (run < 3) return 0;
    *marker_end = marker_pos + run;
    *fence_char = ch;
    return 1;
}

static void scan_markdown_line(CB_ParseTree *tb, const MarkdownSourceMap *map, size_t pos, size_t end,
                               int *in_fence, char *fence_char) {
    size_t marker_end = 0;
    char marker_char = '\0';
    size_t first_nonspace = scan_spaces(map, pos, end);

    if (*in_fence) {
        if (is_fence_line(map, pos, end, &marker_end, &marker_char) && marker_char == *fence_char) {
            if (first_nonspace > pos) add_token(tb, LEXER_WHITESPACE, pos, first_nonspace - pos);
            add_token(tb, LEXER_PREPROCESSOR, first_nonspace, end - first_nonspace);
            *in_fence = 0;
        } else {
            add_token(tb, LEXER_STRING_LITERAL, pos, end - pos);
        }
        return;
    }

    if (is_fence_line(map, pos, end, &marker_end, &marker_char)) {
        if (first_nonspace > pos) add_token(tb, LEXER_WHITESPACE, pos, first_nonspace - pos);
        add_token(tb, LEXER_PREPROCESSOR, first_nonspace, end - first_nonspace);
        *in_fence = 1;
        *fence_char = marker_char;
        return;
    }

    if (first_nonspace > pos) {
        add_token(tb, LEXER_WHITESPACE, pos, first_nonspace - pos);
        pos = first_nonspace;
    }

    if (pos < end && md_char_at(map, pos) == '#') {
        size_t run = 0;
        while (pos + run < end && md_char_at(map, pos + run) == '#') run++;
        if (run <= 6 && (pos + run == end || md_char_at(map, pos + run) == ' ' || md_char_at(map, pos + run) == '\t')) {
            add_token(tb, LEXER_PREPROCESSOR, pos, run);
            pos += run;
        }
    } else if (pos < end && md_char_at(map, pos) == '>') {
        add_token(tb, LEXER_OPERATOR, pos, 1);
        pos++;
    } else if (pos + 1 < end &&
               (md_char_at(map, pos) == '-' || md_char_at(map, pos) == '+' || md_char_at(map, pos) == '*') &&
               (md_char_at(map, pos + 1) == ' ' || md_char_at(map, pos + 1) == '\t')) {
        add_token(tb, LEXER_OPERATOR, pos, 1);
        pos++;
    } else if (pos < end && isdigit((unsigned char)md_char_at(map, pos))) {
        size_t digit_end = pos;
        while (digit_end < end && isdigit((unsigned char)md_char_at(map, digit_end))) digit_end++;
        if (digit_end + 1 < end &&
            (md_char_at(map, digit_end) == '.' || md_char_at(map, digit_end) == ')') &&
            (md_char_at(map, digit_end + 1) == ' ' || md_char_at(map, digit_end + 1) == '\t')) {
            add_token(tb, LEXER_NUMBER_LITERAL, pos, digit_end - pos);
            add_token(tb, LEXER_OPERATOR, digit_end, 1);
            pos = digit_end + 1;
        }
    }

    scan_inline(tb, map, pos, end);
}

static void scan_markdown_tokens(CB_ParseTree *tb, const MarkdownSourceMap *map, size_t start, size_t end) {
    size_t pos = start;
    int in_fence = 0;
    char fence_char = '\0';

    while (pos < end) {
        size_t line_end = pos;
        while (line_end < end && md_char_at(map, line_end) != '\n') line_end++;
        scan_markdown_line(tb, map, pos, line_end, &in_fence, &fence_char);
        if (line_end < end && md_char_at(map, line_end) == '\n') {
            add_token(tb, LEXER_WHITESPACE, line_end, 1);
            line_end++;
        }
        pos = line_end;
    }
}

static CB_NodeType map_cmark_node_type(cmark_node *node) {
    switch (cmark_node_get_type(node)) {
        case CMARK_NODE_DOCUMENT:
            return PARSE_TREE_FILE;
        case CMARK_NODE_BLOCK_QUOTE:
        case CMARK_NODE_LIST:
        case CMARK_NODE_ITEM:
        case CMARK_NODE_FOOTNOTE_DEFINITION:
            return PARSE_TREE_SCOPE;
        case CMARK_NODE_CODE_BLOCK:
        case CMARK_NODE_HTML_BLOCK:
        case CMARK_NODE_CUSTOM_BLOCK:
            return PARSE_TREE_CODEBLOCK;
        case CMARK_NODE_HEADING:
        case CMARK_NODE_THEMATIC_BREAK:
            return PARSE_TREE_STRUCTURE;
        case CMARK_NODE_PARAGRAPH:
        default:
            return PARSE_TREE_STATEMENT;
    }
}

static int cmark_node_range(cmark_node *node, const MarkdownSourceMap *map, size_t *start, size_t *end) {
    int start_line = cmark_node_get_start_line(node);
    int start_column = cmark_node_get_start_column(node);
    int end_line = cmark_node_get_end_line(node);
    int end_column = cmark_node_get_end_column(node);

    if (start_line <= 0 || end_line <= 0) return 0;
    *start = line_column_to_char(map, start_line, start_column, 0);
    *end = line_column_to_char(map, end_line, end_column, 1);
    if (*end <= *start || *start >= map->char_count) return 0;
    if (*end > map->char_count) *end = map->char_count;
    return 1;
}

static cmark_node *parse_markdown_document(const char *source) {
    cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT | CMARK_OPT_SOURCEPOS | CMARK_OPT_VALIDATE_UTF8);
    cmark_node *document;
    static const char *extensions[] = {"table", "strikethrough", "autolink", "tagfilter", "tasklist"};

    if (!parser) return NULL;

    cmark_gfm_core_extensions_ensure_registered();
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        cmark_syntax_extension *extension = cmark_find_syntax_extension(extensions[i]);
        if (extension) cmark_parser_attach_syntax_extension(parser, extension);
    }

    cmark_parser_feed(parser, source, strlen(source));
    document = cmark_parser_finish(parser);
    cmark_parser_free(parser);
    return document;
}

void markdown_parser(CodeBuffer *codeBuffer) {
    char *source;
    MarkdownSourceMap map;
    cmark_node *document;
    CB_ParseTree *tb;
    size_t total_length;
    size_t cursor = 0;

    LOG("markdown_parser: starting parse");
    if (markdown_slow_mode) {
#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
    }

    source = get_code_buffer_source(codeBuffer);
    if (!source) source = strdup("");

    if (source_map_init(&map, source) != 0) {
        source_map_free(&map);
        free(source);
        codeBuffer->parse_tree = NULL;
        return;
    }

    document = parse_markdown_document(source);
    total_length = get_code_buffer_length(codeBuffer);
    if (total_length > map.char_count) total_length = map.char_count;

    tb = cb_create_token_buffer();
    cb_add_child_node(tb, cb_create_node(PARSE_TREE_FILE, 0, total_length));
    cb_set_current_parent_to_root_node(tb);

    if (document) {
        cmark_node *child = cmark_node_first_child(document);
        while (child) {
            size_t start = 0;
            size_t end = 0;
            cmark_node *next = cmark_node_next(child);

            if (cmark_node_range(child, &map, &start, &end) && start >= cursor && start < total_length) {
                if (end > total_length) end = total_length;
                if (start > cursor) scan_markdown_tokens(tb, &map, cursor, start);

                cb_add_child_node(tb, cb_create_node(map_cmark_node_type(child), start, end - start));
                cb_set_current_parent_to_last_node(tb);
                scan_markdown_tokens(tb, &map, start, end);
                cb_set_current_parent_to_grandparent(tb);

                cursor = end;
            }

            child = next;
        }
    }

    if (cursor < total_length) {
        scan_markdown_tokens(tb, &map, cursor, total_length);
    }

    cb_validate_tree(tb);
    codeBuffer->parse_tree = tb;

    if (document) cmark_node_free(document);
    source_map_free(&map);
    free(source);
    LOG("markdown_parser: finished parse");
}
