//
// Created by Adrian Sutherland on 22/05/2025.
// Comms functions
//

#include "token_buffer.h"

// Inproc Comms

// Structure for inproc comms_data
typedef struct {
    CommunicationFunctions *comm;
    CodeBuffer *parser_code_buffer;
} InprocCommsData;

// Inproc function to send an initial load to the parser
static CB_ParseTree * inproc_send_initial_load(CommunicationFunctions *comm_block, InitialLoad *initial_load) {
    // Inproc function to send an initial load to the parser
    // This is a stub for the inproc communication

    // In a real implementation, this would send the initial load to the parser
    printf("Sending initial load to parser: %s\n", initial_load->unique_document_id);
    // Get the comms data
    InprocCommsData *comms_data = (InprocCommsData *)comm_block->comms_data;
    load_initial_content(comms_data->parser_code_buffer, initial_load); // Note this frees the initial_load_copy
    CB_ParseTree *result = comms_data->parser_code_buffer->parse_tree;
    comms_data->parser_code_buffer->parse_tree = NULL; // Disconnect the parser tree from the code buffer
    return result;
}

// Inproc function to send a delta to the parser
static void inproc_send_delta(CommunicationFunctions *comm_block, Delta *delta) {
    // Inproc function to send a delta to the parser
    // This is a stub for the inproc communication
    // In a real implementation, this would send the delta to the parser
    printf("Sending delta to parser: %d\n", (int)delta->change_version);
    // Get the comms data
    InprocCommsData *comms_data = (InprocCommsData *)comm_block->comms_data;
    replay_delta(comms_data->parser_code_buffer, delta);
}

// Inproc function to send a parse result to the editor
static void inproc_send_parse_result(CommunicationFunctions *comm_block, ParseResult *parse_result) {
    // Inproc function to send a parse result to the editor
    // This is a stub for the inproc communication
    // In a real implementation, this would send the parse result to the editor
    printf("Sending parse result to editor: %s\n", parse_result->unique_document_id);
}

// Inproc communications functions factory
CommunicationFunctions* create_inproc_communication_functions(CodeBuffer *parser_cb) {
    CommunicationFunctions *comm = (CommunicationFunctions *)malloc(sizeof(CommunicationFunctions));
    if (comm == NULL) {
        fprintf(stderr, "Failed to allocate memory for CommunicationFunctions\n");
        return NULL;
    }
    comm->send_initial_load = inproc_send_initial_load;
    comm->send_delta = inproc_send_delta;
    comm->send_parse_result = inproc_send_parse_result;

    // Create comms_data
    InprocCommsData *comms_data = (InprocCommsData *)malloc(sizeof(InprocCommsData));
    if (comms_data == NULL) {
        fprintf(stderr, "Failed to allocate memory for InprocCommsData\n");
        free(comm);
        return NULL;
    }
    comms_data->comm = comm;
    comms_data->parser_code_buffer = parser_cb;
    comm->comms_data = comms_data;

    return comm;
}

// Free the inproc communication functions
void free_inproc_communication_functions(CommunicationFunctions *comm) {
    if (comm == NULL) return;

    // Free comms_data
    InprocCommsData *comms_data = (InprocCommsData *)comm->comms_data;
    if (comms_data != NULL) {
        free(comms_data);
    }

    // Free the communication functions
    free(comm);
}
