#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"
#include "thread_utils.h"
#include "dslsyntax_log.h"

// Print tree recursively
static void print_tree(CB_Node *node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("Node: type=%d, pos=%zu, len=%zu, sev=%d\n", 
           node->type, node->pos, node->length, node->severity);
    if (node->message) {
        for (int i = 0; i < depth + 2; i++) printf("  ");
        printf("Message: %s\n", node->message);
    }
    
    CB_Node *child = node->child;
    while (child) {
        print_tree(child, depth + 1);
        child = child->sibling;
    }
}

#include <time.h>

static int wait_for_parser(CodeBuffer *cb) {
    time_t start_time = time(NULL);
    printf("Waiting for parser...\n");
    while (editor_is_parsing_thread_active()) {
        if (time(NULL) - start_time > 10) {
            printf("Error: Parser timed out after 10 seconds. Killing process...\n");
            cb_kill_parser_process(cb);
            return 0; // Timeout
        }
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    return 1; // Success
}

int main(int argc, char **argv) {
    char cmd_buf[2048];
    CodeBuffer *cb = NULL;
    
    cb_log_init(NULL);
    editor_init();
    
    printf("Parser Tester REPL. Commands:\n");
    printf("  INIT <parser_cmd> <source_file>\n");
    printf("  INSERT <line> <col> <text...>\n");
    printf("  DELETE <line> <col> <count>\n");
    printf("  DUMP_AST\n");
    printf("  STATUS\n");
    printf("  QUIT\n\n");
    
    while (fgets(cmd_buf, sizeof(cmd_buf), stdin)) {
        char *nl = strchr(cmd_buf, '\n');
        if (nl) *nl = '\0';
        if (strlen(cmd_buf) == 0) continue;
        
        char *tok = strtok(cmd_buf, " ");
        if (!tok) continue;
        
        if (strcmp(tok, "QUIT") == 0) {
            break;
        } else if (strcmp(tok, "INIT") == 0) {
            char *parser_cmd = NULL;
            char *source_file = NULL;

            // Check if the next character is a quote
            char *next_token_start = cmd_buf + (tok - cmd_buf) + strlen(tok) + 1;
            while (*next_token_start == ' ') next_token_start++;

            if (*next_token_start == '"') {
                parser_cmd = next_token_start + 1;
                char *quote_end = strchr(parser_cmd, '"');
                if (quote_end) {
                    *quote_end = '\0';
                    source_file = strtok(quote_end + 1, " \n\r");
                }
            } else {
                parser_cmd = strtok(NULL, " \n\r");
                source_file = strtok(NULL, " \n\r");
            }

            if (!parser_cmd || !source_file) {
                printf("Error: INIT requires <parser_cmd> and <source_file>. Use quotes if parser_cmd has spaces.\n");
                continue;
            }

            printf("INIT command: %s\n", parser_cmd);
            printf("INIT source : %s\n", source_file);            
            if (cb) {
                // Free old
                free_code_buffer(cb);
                cb = NULL;
            }
            
            char full_cmd[1024];
            snprintf(full_cmd, sizeof(full_cmd), "%s -d --parser", parser_cmd);
            
            CommunicationFunctions *comm = create_stdio_communication_functions(full_cmd);
            if (!comm) {
                printf("Failed to create comms\n");
                continue;
            }
            
            cb = create_code_buffer(comm, NULL);
            cb_set_auto_relaunch(cb, 0); // Strict mode!
            
            FILE *f = fopen(source_file, "r");
            if (!f) {
                printf("Failed to open %s\n", source_file);
                continue;
            }
            
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            char *content = malloc(fsize + 1);
            fread(content, 1, fsize, f);
            content[fsize] = '\0';
            fclose(f);
            
            InitialLoad *load = create_initial_load(source_file, content);
            free(content);
            
            load_initial_content(cb, load);
            if (wait_for_parser(cb)) {
                printf("OK: Initialized\n");
            }
            
        } else if (strcmp(tok, "INSERT") == 0) {
            if (!cb) { printf("Error: Not initialized\n"); continue; }
            
            char *line_s = strtok(NULL, " ");
            char *col_s = strtok(NULL, " ");
            char *text = strtok(NULL, ""); // rest of string
            
            if (!line_s || !col_s || !text) {
                printf("Error: INSERT <line> <col> <text>\n");
                continue;
            }
            
            Transaction t;
            t.type = TRANSACTION_ADDCHARS;
            t.pos_line = atoi(line_s);
            t.pos_col = atoi(col_s);
            t.content = text; // Just points into cmd_buf
            t.count = 0;
            
            enter_codeblock_critical_section();
            editor_apply_transaction(cb, t);
            exit_codeblock_critical_section();
            
            process_delta(cb);
            if (wait_for_parser(cb)) {
                printf("OK: Inserted\n");
            }
            
        } else if (strcmp(tok, "DELETE") == 0) {
            if (!cb) { printf("Error: Not initialized\n"); continue; }
            
            char *line_s = strtok(NULL, " ");
            char *col_s = strtok(NULL, " ");
            char *count_s = strtok(NULL, " ");
            
            if (!line_s || !col_s || !count_s) {
                printf("Error: DELETE <line> <col> <count>\n");
                continue;
            }
            
            Transaction t;
            t.type = TRANSACTION_DELETECHARS;
            t.pos_line = atoi(line_s);
            t.pos_col = atoi(col_s);
            t.content = NULL;
            t.count = atoi(count_s);
            
            enter_codeblock_critical_section();
            editor_apply_transaction(cb, t);
            exit_codeblock_critical_section();
            
            process_delta(cb);
            if (wait_for_parser(cb)) {
                printf("OK: Deleted\n");
            }
            
        } else if (strcmp(tok, "DUMP_AST") == 0) {
            if (!cb) { printf("Error: Not initialized\n"); continue; }
            enter_codeblock_critical_section();
            if (cb->parse_tree) {
                print_tree(cb->parse_tree->root, 0);
            } else {
                printf("No Parse Tree\n");
            }
            exit_codeblock_critical_section();
            printf("OK: Dumped\n");
            
        } else if (strcmp(tok, "STATUS") == 0) {
            if (!cb) { printf("Error: Not initialized\n"); continue; }
            enter_codeblock_critical_section();
            CB_ParserState state = cb_get_parser_state(cb);
            exit_codeblock_critical_section();
            
            switch (state) {
                case CB_PARSER_NOT_LOADED: printf("STATUS: NOT_LOADED\n"); break;
                case CB_PARSER_ACTIVE: printf("STATUS: ACTIVE\n"); break;
                case CB_PARSER_CRASHED: printf("STATUS: CRASHED\n"); break;
                case CB_PARSER_SUSPENDED: printf("STATUS: SUSPENDED\n"); break;
                default: printf("STATUS: UNKNOWN\n"); break;
            }
            printf("OK: Status\n");
            
        } else {
            printf("Error: Unknown command %s\n", tok);
        }
    }
    
    if (cb) free_code_buffer(cb);
    editor_free();
    
    return 0;
}