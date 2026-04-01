#ifndef DSLSYNTAX_SERIALIZATION_H
#define DSLSYNTAX_SERIALIZATION_H

#include "dslsyntax_common.h"

/* Structure for a serialized token in the protocol stream */
typedef struct CB_SerializedToken {
    CB_NodeType type;
    size_t pos;
    size_t length;
    int identifier_id;
    CB_Severity severity;
    char *message_code;
    char *message;
} CB_SerializedToken;

/* Stream of serialized tokens */
typedef struct CB_TokenStream {
    CB_SerializedToken *tokens;
    size_t count;
} CB_TokenStream;

/* Function to flatten a CB_ParseTree into a CB_TokenStream */
CB_TokenStream* cb_flatten_tree(CB_ParseTree *tb);

/* Function to reconstruct a CB_ParseTree from a CB_TokenStream */
CB_ParseTree* cb_reconstruct_tree(CB_TokenStream *stream);

/* Helper to free a CB_TokenStream */
void cb_free_token_stream(CB_TokenStream *stream);

/* 
 * Delta Serialization 
 * (For Editor -> Parser communication)
 */

/* Serializes a Delta into a string (caller must free) */
char* cb_serialize_delta(Delta *delta);

/* Deserializes a Delta from a string */
Delta* cb_deserialize_delta(const char *data);

/* 
 * TokenStream Serialization 
 * (For Parser -> Editor communication)
 */

/* Serializes a TokenStream into a string (caller must free) */
char* cb_serialize_token_stream(CB_TokenStream *stream);

/* Deserializes a TokenStream from a string */
CB_TokenStream* cb_deserialize_token_stream(const char *data);

/* 
 * InitialLoad Serialization
 */
char* cb_serialize_initial_load(InitialLoad *load);
InitialLoad* cb_deserialize_initial_load(const char *data);

/* 
 * Socket Communication Functions
 */

/* Creates communication functions for a client connecting to a socket */
CommunicationFunctions* create_socket_communication_functions(const char *address, int port);

/* 
 * Server Loop for the Parser
 * This function blocks and handles client connections.
 * It calls the parser's logic for each request.
 */
void cb_start_server(CodeBuffer *parser_cb, const char *address, int port);
void cb_start_stdio_server(CodeBuffer *parser_cb);

#endif /* DSLSYNTAX_SERIALIZATION_H */
