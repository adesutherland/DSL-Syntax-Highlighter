#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <time.h>

#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"
#include "thread_utils.h"
#include "dslsyntax_log.h"

static int g_verbose = 1;

// Print tree recursively with detailed information
static void print_tree(CodeBuffer *cb, CB_Node *node, int depth, FILE *out) {
    if (!node) return;
    for (int i = 0; i < depth; i++) fprintf(out, "  ");
    
    char *value = NULL;
    // Extract text for leaf nodes
    if (node->type < PARSE_TREE) {
        get_code_buffer_part(cb, node->pos, node->length, NULL, NULL, &value);
    }

    const char* type_name = cb_token_type_to_string(node->type);
    fprintf(out, "Node: type=%d (%s), pos=%zu, len=%zu, sev=%d", 
            (int)node->type, type_name ? type_name : "UNKNOWN", 
            node->pos, node->length, (int)node->severity);
            
    if (value) {
        fprintf(out, ", text=\"%s\"", value);
        free(value);
    }
    fprintf(out, "\n");

    if (node->message) {
        for (int i = 0; i < depth + 2; i++) fprintf(out, "  ");
        fprintf(out, "Message: %s\n", node->message);
    }
    
    CB_Node *child = node->child;
    while (child) {
        print_tree(cb, child, depth + 1, out);
        child = child->sibling;
    }
}

static int wait_for_parser(CodeBuffer *cb) {
    time_t start_time = time(NULL);
    if (g_verbose) printf("Waiting for parser...\n");
    while (editor_is_parsing_thread_active()) {
        if (time(NULL) - start_time > 10) {
            if (g_verbose) printf("Error: Parser timed out after 10 seconds. Killing process...\n");
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

static void print_help() {
    printf("Usage: parser_tester [options] [script_file]\n");
    printf("Options:\n");
    printf("  -p <cmd>    Parser command (if provided, automatically initializes)\n");
    printf("  -s <file>   Source file to load\n");
    printf("  -g <file>   Golden output file to compare against DUMP_AST\n");
    printf("  -q          Quiet mode (suppress \"Waiting for parser...\" messages)\n");
    printf("  -h, --help  Show this help\n\n");
    printf("REPL Commands (via script or stdin):\n");
    printf("  INIT <parser_cmd> <source_file>\n");
    printf("  INSERT <line> <col> <text...>\n");
    printf("  INSERT_ASYNC <line> <col> <text...>\n");
    printf("  DELETE <line> <col> <count>\n");
    printf("  DELETE_ASYNC <line> <col> <count>\n");
    printf("  SYNC\n");
    printf("  DUMP_AST\n");
    printf("  STATUS\n");
    printf("  LOG <message...>\n");
    printf("  QUIT\n");
}

#ifndef _WIN32
#include <signal.h>
static void watchdog_handler(int sig) {
    (void)sig;
    fprintf(stderr, "\nError: Global watchdog timer expired (20s). Force exiting.\n");
    exit(1);
}
#endif

int main(int argc, char **argv) {
#ifndef _WIN32
    signal(SIGALRM, watchdog_handler);
    alarm(20); // Hard 20s limit for the whole test
#endif
    char cmd_buf[4096];
    CodeBuffer *cb = NULL;
    char *parser_cmd_opt = NULL;
    char *source_file_opt = NULL;
    char *golden_file_opt = NULL;
    char *script_file_opt = NULL;
    FILE *accumulated_out = NULL;
    char temp_filename[1024];
    
    cb_log_init(NULL);
    editor_init();

    // Basic arg parsing
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            parser_cmd_opt = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            source_file_opt = argv[++i];
        } else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            golden_file_opt = argv[++i];
        } else if (strcmp(argv[i], "-q") == 0) {
            g_verbose = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (argv[i][0] != '-') {
            script_file_opt = argv[i];
        }
    }

    if (golden_file_opt) {
        snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", golden_file_opt);
        accumulated_out = fopen(temp_filename, "w");
        if (!accumulated_out) {
            fprintf(stderr, "Error: Could not open %s for writing\n", temp_filename);
            return 1;
        }
    }

    FILE *input = stdin;
    if (script_file_opt) {
        input = fopen(script_file_opt, "r");
        if (!input) {
            fprintf(stderr, "Error: Could not open script file %s\n", script_file_opt);
            if (accumulated_out) fclose(accumulated_out);
            return 1;
        }
    }

    if (g_verbose && !script_file_opt && !parser_cmd_opt) {
        printf("Parser Tester REPL. Use -h for help.\n");
    }

    // Auto-init if args provided
    int skip_input = 0;
    if (parser_cmd_opt && source_file_opt) {
        snprintf(cmd_buf, sizeof(cmd_buf), "INIT \"%s\" %s", parser_cmd_opt, source_file_opt);
        skip_input = 1;
    }

    while (skip_input || fgets(cmd_buf, sizeof(cmd_buf), input)) {
        if (skip_input) {
            // Simulated INIT command already in cmd_buf
            skip_input = 0;
        } else {
            char *nl = strchr(cmd_buf, '\n');
            if (nl) *nl = '\0';
            if (strlen(cmd_buf) == 0) continue;
        }
        
        char *cmd_ptr = cmd_buf;
        while (*cmd_ptr == ' ') cmd_ptr++;
        if (*cmd_ptr == '#' || *cmd_ptr == '\0') continue; // Skip comments and empty lines

        if (accumulated_out) {
            fprintf(accumulated_out, "COMMAND: %s\n", cmd_ptr);
        }

        char *tok = strtok(cmd_ptr, " ");
        if (!tok) continue;
        
        if (strcmp(tok, "QUIT") == 0) {
            break;
        } else if (strcmp(tok, "INIT") == 0) {
            char *parser_cmd = NULL;
            char *source_file = NULL;

            // Find the start of the next token manually to handle quotes
            char *next = cmd_ptr + strlen(tok) + 1;
            while (*next == ' ' || *next == '\0') {
                if (*next == '\0' && (size_t)(next - cmd_buf) >= sizeof(cmd_buf)) break;
                if (*next == '\0') { next++; continue; }
                next++;
            }

            if (*next == '"') {
                parser_cmd = next + 1;
                char *end = strchr(parser_cmd, '"');
                if (end) {
                    *end = '\0';
                    source_file = strtok(end + 1, " \n\r");
                }
            } else {
                parser_cmd = strtok(next, " \n\r");
                source_file = strtok(NULL, " \n\r");
            }

            if (!parser_cmd || !source_file) {
                fprintf(stderr, "Error: INIT requires <parser_cmd> and <source_file>\n");
                continue;
            }

            if (g_verbose) {
                printf("INIT command: %s\n", parser_cmd);
                printf("INIT source : %s\n", source_file);
            }
            if (cb) free_code_buffer(cb);
            
            char full_cmd[2048];
            snprintf(full_cmd, sizeof(full_cmd), "%s -d --syntaxhighlight", parser_cmd);
            
            CommunicationFunctions *comm = create_stdio_communication_functions(full_cmd);
            if (!comm) {
                fprintf(stderr, "Failed to create comms\n");
                continue;
            }
            
            cb = create_code_buffer(comm, NULL);
            cb_set_auto_relaunch(cb, 0); 
            
            FILE *f = fopen(source_file, "r");
            if (!f) {
                fprintf(stderr, "Failed to open %s\n", source_file);
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
                if (g_verbose) printf("OK: Initialized\n");
            }
            
        } else if (strcmp(tok, "INSERT") == 0 || strcmp(tok, "INSERT_ASYNC") == 0) {
            int is_async = (strcmp(tok, "INSERT_ASYNC") == 0);
            if (!cb) { fprintf(stderr, "Error: Not initialized\n"); continue; }
            
            char *line_s = strtok(NULL, " ");
            char *col_s = strtok(NULL, " ");
            char *text = strtok(NULL, ""); 
            
            if (!line_s || !col_s || !text) {
                fprintf(stderr, "Error: INSERT <line> <col> <text>\n");
                continue;
            }
            
            Transaction t;
            t.type = TRANSACTION_ADDCHARS;
            t.pos_line = atoi(line_s);
            t.pos_col = atoi(col_s);
            t.content = text;
            t.count = 0;
            
            enter_codeblock_critical_section();
            editor_apply_transaction(cb, t);
            exit_codeblock_critical_section();
            
            process_delta(cb);
            if (!is_async) {
                if (wait_for_parser(cb)) {
                    if (g_verbose) printf("OK: Inserted\n");
                }
            } else {
                if (g_verbose) printf("OK: Inserted (Async)\n");
            }
            
        } else if (strcmp(tok, "DELETE") == 0 || strcmp(tok, "DELETE_ASYNC") == 0) {
            int is_async = (strcmp(tok, "DELETE_ASYNC") == 0);
            if (!cb) { fprintf(stderr, "Error: Not initialized\n"); continue; }
            
            char *line_s = strtok(NULL, " ");
            char *col_s = strtok(NULL, " ");
            char *count_s = strtok(NULL, " ");
            
            if (!line_s || !col_s || !count_s) {
                fprintf(stderr, "Error: DELETE <line> <col> <count>\n");
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
            if (!is_async) {
                if (wait_for_parser(cb)) {
                    if (g_verbose) printf("OK: Deleted\n");
                }
            } else {
                if (g_verbose) printf("OK: Deleted (Async)\n");
            }
            
        } else if (strcmp(tok, "SYNC") == 0) {
            if (!cb) { fprintf(stderr, "Error: Not initialized\n"); continue; }
            if (wait_for_parser(cb)) {
                if (g_verbose) printf("OK: Synced\n");
            }
        } else if (strcmp(tok, "DUMP_AST") == 0) {
            if (!cb) { fprintf(stderr, "Error: Not initialized\n"); continue; }
            
            FILE *out = accumulated_out ? accumulated_out : stdout;

            enter_codeblock_critical_section();
            if (cb->parse_tree) {
                print_tree(cb, cb->parse_tree->root, 0, out);
            } else {
                fprintf(out, "No Parse Tree\n");
            }
            exit_codeblock_critical_section();

            if (g_verbose) printf("OK: Dumped\n");
            
        } else if (strcmp(tok, "LOG") == 0) {
            char *msg = strtok(NULL, "");
            if (msg) LOG("TESTER: %s", msg);
            if (g_verbose) printf("OK: Logged\n");
            
        } else if (strcmp(tok, "STATUS") == 0) {
            if (!cb) { fprintf(stderr, "Error: Not initialized\n"); continue; }
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
            if (g_verbose) printf("OK: Status\n");
            
        } else {
            fprintf(stderr, "Error: Unknown command %s\n", tok);
        }
    }
    
    if (cb) free_code_buffer(cb);
    if (input != stdin) fclose(input);

    if (golden_file_opt && accumulated_out) {
        fclose(accumulated_out);
        // Compare with golden file
        FILE *f1 = fopen(temp_filename, "r");
        FILE *f2 = fopen(golden_file_opt, "r");
        if (!f2) {
            printf("Golden file %s missing. Creating it from current output.\n", golden_file_opt);
            rename(temp_filename, golden_file_opt);
        } else {
            int match = 1;
            int c1, c2;
            while ((c1 = fgetc(f1)) != EOF && (c2 = fgetc(f2)) != EOF) {
                if (c1 != c2) { match = 0; break; }
            }
            if (match && (fgetc(f1) != EOF || fgetc(f2) != EOF)) match = 0;
            fclose(f1);
            fclose(f2);
            if (match) {
                if (g_verbose) printf("OK: Output matches %s\n", golden_file_opt);
                remove(temp_filename);
            } else {
                printf("FAIL: Output does NOT match %s\n", golden_file_opt);
                editor_free();
                return 1;
            }
        }
    }

    editor_free();
    
    return 0;
}
