#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dslsyntax_common.h"
#include "serialization.h"
#include "dslsyntax_log.h"

void print_tree(CB_Node *node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("Node: type=%d (%s), pos=%zu, len=%zu, sev=%c\n", 
           (int)node->type, cb_token_type_to_string(node->type), node->pos, node->length, (char)node->severity);
    
    CB_Node *child = node->child;
    while (child) {
        print_tree(child, depth + 1);
        child = child->sibling;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s filename [port]\n", argv[0]);
        return 1;
    }
    char *filename = argv[1];
    int port = 8080;
    if (argc > 2) port = atoi(argv[2]);

    cb_log_init(NULL); // Log to stderr

    printf("Connecting to parser on port %d...\n", port);
    CommunicationFunctions *comm = create_socket_communication_functions("127.0.0.1", port);

    /* Read file */
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        return 1;
    }
    char line_buf[1024];
    InitialLoad load;
    load.unique_document_id = strdup(filename);
    load.change_version = 0;
    load.line_count = 0;
    load.lines = NULL;

    while (fgets(line_buf, sizeof(line_buf), f)) {
        char *nl = strchr(line_buf, '\n');
        if (nl) *nl = '\0';
        load.lines = realloc(load.lines, (load.line_count + 1) * sizeof(CodeBufferLine));
        utf8_to_line(line_buf, &load.lines[load.line_count++]);
    }
    fclose(f);

    printf("Sending Initial Load (%zu lines)...\n", load.line_count);
    CB_ParseTree *tb = comm->send_initial_load(comm, &load);

    if (!tb) {
        printf("Failed to get parse tree from parser.\n");
        return 1;
    }

    printf("Parse Tree received:\n");
    print_tree(tb->root, 0);

    /* Cleanup */
    cb_free_token_buffer(tb);
    /* load cleanup omitted for brevity in test tool */
    
    return 0;
}
