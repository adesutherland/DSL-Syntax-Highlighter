#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser_highlighter.h"
#include "token_buffer.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage: %s filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Read the entire file into memory
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);
    char *source_code = malloc(file_size + 1);
    fread(source_code, 1, file_size, fp);
    source_code[file_size] = '\0';
    fclose(fp);

    // Highlight - creating the codebuffer and tokenbuffer
    CB_ParseTree *tb = NULL;
    CodeBuffer *cb = NULL;
    highlight_init(source_code, &cb, &tb);

    // Print the CB_ParseTree
    cb_print_token_buffer(cb, tb);

    // Free resources
    cb_free_token_buffer(tb);
    free_code_buffer(cb);
    free(source_code);

    return 0;
}
