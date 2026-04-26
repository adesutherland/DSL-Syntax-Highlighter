#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dslsyntax_common.h"
#include "serialization.h"

static InitialLoad *make_single_line_load(const char *document_id, const char *line, size_t version) {
    InitialLoad *load = (InitialLoad*)calloc(1, sizeof(InitialLoad));
    assert(load != NULL);

    load->unique_document_id = strdup(document_id);
    load->change_version = version;
    load->line_count = 1;
    load->lines = (CodeBufferLine*)malloc(sizeof(CodeBufferLine));
    assert(load->unique_document_id != NULL);
    assert(load->lines != NULL);
    utf8_to_line(line, &load->lines[0]);
    return load;
}

static Delta make_insert_delta(const char *document_id,
                               size_t base_version,
                               size_t change_version,
                               int pos_col,
                               const char *content,
                               const char *overlay_id) {
    Delta delta;
    memset(&delta, 0, sizeof(delta));
    delta.unique_document_id = strdup(document_id);
    delta.base_version = base_version;
    delta.change_version = change_version;
    delta.overlay_id = overlay_id ? strdup(overlay_id) : NULL;
    delta.transaction_count = 1;
    delta.transactions = (Transaction*)calloc(1, sizeof(Transaction));
    assert(delta.unique_document_id != NULL);
    assert(!overlay_id || delta.overlay_id != NULL);
    assert(delta.transactions != NULL);
    delta.transactions[0].type = TRANSACTION_ADDCHARS;
    delta.transactions[0].pos_line = 0;
    delta.transactions[0].pos_col = pos_col;
    delta.transactions[0].content = strdup(content);
    assert(delta.transactions[0].content != NULL);
    return delta;
}

static void clear_stack_delta(Delta *delta) {
    if (!delta) return;
    free(delta->unique_document_id);
    free(delta->overlay_id);
    if (delta->transactions) {
        for (size_t i = 0; i < delta->transaction_count; i++) {
            free(delta->transactions[i].content);
        }
        free(delta->transactions);
    }
    memset(delta, 0, sizeof(*delta));
}

static size_t parse_root_length(CB_ParseTree *tree) {
    assert(tree != NULL);
    assert(tree->root != NULL);
    return tree->root->length;
}

static void assert_initial_load(CommunicationFunctions *comm,
                                const char *document_id,
                                const char *line,
                                size_t version,
                                size_t expected_root_length) {
    InitialLoad *load = make_single_line_load(document_id, line, version);
    CB_ParseTree *tree = comm->send_initial_load(comm, load);
    assert(tree != NULL);
    assert(parse_root_length(tree) == expected_root_length);
    cb_free_token_buffer(tree);
    free_initial_load(load);
}

static void assert_delta(CommunicationFunctions *comm,
                         Delta *delta,
                         size_t expected_root_length) {
    CB_ParseTree *tree = comm->send_delta(comm, delta);
    assert(tree != NULL);
    assert(parse_root_length(tree) == expected_root_length);
    cb_free_token_buffer(tree);
}

static void assert_hypothesis(CommunicationFunctions *comm,
                              Delta *delta,
                              size_t expected_root_length) {
    CB_ParseTree *tree = comm->send_hypothesis(comm, delta);
    assert(tree != NULL);
    assert(parse_root_length(tree) == expected_root_length);
    cb_free_token_buffer(tree);
}

static void test_delta_v2_wire_format(void) {
    Delta delta = make_insert_delta("dir/a.toy", 7, 8, 1, "|x|", "completion|probe");
    char *wire = cb_serialize_delta(&delta);
    Delta *roundtrip;

    assert(wire != NULL);
    assert(strncmp(wire, "2|", 2) == 0);

    roundtrip = cb_deserialize_delta(wire);
    assert(roundtrip != NULL);
    assert(roundtrip->unique_document_id != NULL);
    assert(strcmp(roundtrip->unique_document_id, "dir/a.toy") == 0);
    assert(roundtrip->base_version == 7);
    assert(roundtrip->change_version == 8);
    assert(roundtrip->overlay_id != NULL);
    assert(strcmp(roundtrip->overlay_id, "completion|probe") == 0);
    assert(roundtrip->transaction_count == 1);
    assert(roundtrip->transactions[0].type == TRANSACTION_ADDCHARS);
    assert(roundtrip->transactions[0].pos_col == 1);
    assert(strcmp(roundtrip->transactions[0].content, "|x|") == 0);

    free_delta(roundtrip);
    free(wire);
    clear_stack_delta(&delta);
}

static void test_multi_document_and_hypothesis_protocol(const char *parser_command) {
    CommunicationFunctions *comm = create_stdio_communication_functions(parser_command);
    assert(comm != NULL);
    assert(comm->send_initial_load != NULL);
    assert(comm->send_delta != NULL);
    assert(comm->send_hypothesis != NULL);

    assert_initial_load(comm, "/tmp/project/a.toy", "a", 1, 2);
    assert_initial_load(comm, "/tmp/project/b.toy", "bbbb", 1, 5);

    Delta doc_a_delta = make_insert_delta("/tmp/project/a.toy", 1, 2, 1, "aa", NULL);
    assert_delta(comm, &doc_a_delta, 4);
    clear_stack_delta(&doc_a_delta);

    Delta doc_b_hypothesis = make_insert_delta("/tmp/project/b.toy", 1, 2, 4, "HHH", "completion");
    assert_hypothesis(comm, &doc_b_hypothesis, 8);
    clear_stack_delta(&doc_b_hypothesis);

    Delta doc_b_delta = make_insert_delta("/tmp/project/b.toy", 1, 2, 4, "c", NULL);
    assert_delta(comm, &doc_b_delta, 6);
    clear_stack_delta(&doc_b_delta);

    Delta stale_doc_b_hypothesis = make_insert_delta("/tmp/project/b.toy", 1, 3, 5, "stale", "stale-completion");
    CB_ParseTree *stale_tree = comm->send_hypothesis(comm, &stale_doc_b_hypothesis);
    assert(stale_tree == NULL);
    clear_stack_delta(&stale_doc_b_hypothesis);

    free_stdio_communication_functions(comm);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <protocol_test_parser>\n", argv[0]);
        return 2;
    }

    test_delta_v2_wire_format();
    test_multi_document_and_hypothesis_protocol(argv[1]);

    printf("Protocol tests passed.\n");
    return 0;
}
