#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#define ACCESS _access
/* MinGW/MSVC usually define X_OK in <io.h>. Guard to avoid redefinition warnings. */
#ifndef X_OK
#define X_OK 1 /* execute permission */
#endif
#else
#include <limits.h>
#include <unistd.h>
#define ACCESS access
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif
#include "dslsyntax_common.h"
#include "dslsyntax_editor.h"
#include "serialization.h"

#ifndef _WIN32
static void test_stdio_quoted_parser_path(const char *parser_cmd) {
    char dir_template[PATH_MAX];
    char parser_path[PATH_MAX];
    char linked_path[PATH_MAX];
    char quoted_cmd[PATH_MAX * 2];
    const char *tmpdir = getenv("TMPDIR");
    char *created_dir;
    char *resolved_path;
    int written;
    int symlink_rc;

    printf("Testing quoted parser command path...\n");

    if (!tmpdir) tmpdir = "/tmp";
    written = snprintf(dir_template, sizeof(dir_template), "%s/dslsh parser test.XXXXXX", tmpdir);
    assert(written >= 0 && written < (int)sizeof(dir_template));
    created_dir = mkdtemp(dir_template);
    assert(created_dir != NULL);
    resolved_path = realpath(parser_cmd, parser_path);
    assert(resolved_path != NULL);
    written = snprintf(linked_path, sizeof(linked_path), "%s/tp with spaces", dir_template);
    assert(written >= 0 && written < (int)sizeof(linked_path));
    symlink_rc = symlink(parser_path, linked_path);
    assert(symlink_rc == 0);
    written = snprintf(quoted_cmd, sizeof(quoted_cmd), "\"%s\" -d", linked_path);
    assert(written >= 0 && written < (int)sizeof(quoted_cmd));

    CommunicationFunctions *comm = create_stdio_communication_functions(quoted_cmd);
    assert(comm != NULL);

    InitialLoad *load = create_initial_load("quoted_test_doc", "say quoted");

    CB_ParseTree *tb = comm->send_initial_load(comm, load);
    free_initial_load(load);
    assert(tb != NULL);
    assert(tb->root != NULL);

    cb_free_token_buffer(tb);
    free_stdio_communication_functions(comm);
    unlink(linked_path);
    rmdir(dir_template);

    printf("Quoted parser command path test successful!\n");
}
#endif

void test_stdio_communication() {
    printf("Testing stdio communication...\n");

    /* 
     * In the build environment, toyparser/tp.exe should be accessible.
     * We assume this test is run from the project root or the build directory.
     * Try to locate the parser executable.
     */
#ifdef _WIN32
    const char *candidates[] = {
        "toyparser/tp.exe",
        "cmake-build-debug/toyparser/tp.exe",
        "../toyparser/tp.exe",
        "../../toyparser/tp.exe"
    };
#else
    const char *candidates[] = {
        "toyparser/tp",
        "cmake-build-debug/toyparser/tp",
        "../toyparser/tp",
        "../../toyparser/tp"
    };
#endif
    
    const char *parser_cmd = NULL;
    for (int i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (ACCESS(candidates[i], X_OK) == 0) {
            parser_cmd = candidates[i];
            break;
        }
    }

    if (parser_cmd == NULL) {
        fprintf(stderr, "Failed to find parser executable (tp). Checked common locations.\n");
        return;
    }
    printf("Using parser executable: %s\n", parser_cmd);

    /* Create Client Communication */
    CommunicationFunctions *comm = create_stdio_communication_functions(parser_cmd);
    if (comm == NULL) {
        fprintf(stderr, "Failed to create stdio communication functions. Is %s available?\n", parser_cmd);
        return;
    }

    /* 1. Test Initial Load */
    printf("Client: Sending Initial Load...\n");
    InitialLoad *load = create_initial_load("test_doc", "Line 1\nLine 2");

    CB_ParseTree *tb = comm->send_initial_load(comm, load);
    free_initial_load(load);
    if (tb) {
        printf("Client: Initial Load Result received. Tree root type: %d\n", tb->root->type);
    } else {
        printf("Client: Initial Load failed.\n");
    }

    /* 2. Test non-committing hypothesis */
    if (tb) {
        printf("Client: Sending Hypothesis Delta...\n");
        Delta hypothesis;
        memset(&hypothesis, 0, sizeof(hypothesis));
        hypothesis.unique_document_id = strdup("test_doc");
        hypothesis.base_version = 1;
        hypothesis.change_version = 2;
        hypothesis.overlay_id = strdup("completion");
        hypothesis.transaction_count = 1;
        hypothesis.transactions = (Transaction*)malloc(sizeof(Transaction));
        hypothesis.transactions[0].type = TRANSACTION_ADDCHARS;
        hypothesis.transactions[0].pos_line = 0;
        hypothesis.transactions[0].pos_col = 6;
        hypothesis.transactions[0].count = 0;
        hypothesis.transactions[0].content = strdup(" XYZXYZ");

        CB_ParseTree *hypothesis_tree = comm->send_hypothesis(comm, &hypothesis);
        if (hypothesis_tree) {
            printf("Client: Hypothesis Result received.\n");
            cb_free_token_buffer(hypothesis_tree);
        } else {
            printf("Client: Hypothesis send failed.\n");
        }
        free(hypothesis.unique_document_id);
        free(hypothesis.overlay_id);
        free(hypothesis.transactions[0].content);
        free(hypothesis.transactions);
    }

    /* 3. Test Delta. This must still apply from version 1, proving H did not commit. */
    if (tb) {
        printf("Client: Sending Delta...\n");
        Delta delta;
        memset(&delta, 0, sizeof(delta));
        delta.unique_document_id = strdup("test_doc");
        delta.base_version = 1;
        delta.change_version = 2;
        delta.overlay_id = NULL;
        delta.transaction_count = 1;
        delta.transactions = (Transaction*)malloc(sizeof(Transaction));
        delta.transactions[0].type = TRANSACTION_ADDCHARS;
        delta.transactions[0].pos_line = 0;
        delta.transactions[0].pos_col = 6;
        delta.transactions[0].count = 0;
        delta.transactions[0].content = strdup(" Added");

        CB_ParseTree *tb2 = comm->send_delta(comm, &delta);
        free(delta.unique_document_id);
        free(delta.transactions[0].content);
        free(delta.transactions);
        if (tb2) {
            printf("Client: Delta Result received. Root length: %zu\n", tb2->root->length);
            cb_free_token_buffer(tb2);
        } else {
            printf("Client: Delta send failed.\n");
        }
    }

    /* Cleanup client */
    if (tb) cb_free_token_buffer(tb);
    free_stdio_communication_functions(comm);

#ifndef _WIN32
    test_stdio_quoted_parser_path(parser_cmd);
#endif
    
    printf("Stdio communication test successful!\n");
}

int main() {
    test_stdio_communication();
    return 0;
}
