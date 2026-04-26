#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serialization.h"
#include "dslsyntax_log.h"

/* --- Flatten / Reconstruct Tree --- */

/* Recursive helper to flatten the tree */
static void cb_flatten_node(CB_Node *node, CB_TokenStream *stream) {
    if (node == NULL) return;

    /* Add the node itself as a TREE_DOWN if it has children */
    if (node->child != NULL) {
        stream->tokens = (CB_SerializedToken*)safe_realloc(stream->tokens, (stream->count + 1) * sizeof(CB_SerializedToken));
        CB_SerializedToken *st = &stream->tokens[stream->count++];
        st->type = TREE_DOWN;
        st->pos = node->pos;
        st->length = node->length;
        st->identifier_id = node->identifier_id;
        st->severity = node->severity;
        st->message_code = node->message_code ? strdup(node->message_code) : NULL;
        st->message = node->message ? strdup(node->message) : NULL;

        /* Now add its actual type as a marker */
        stream->tokens = (CB_SerializedToken*)safe_realloc(stream->tokens, (stream->count + 1) * sizeof(CB_SerializedToken));
        st = &stream->tokens[stream->count++];
        st->type = node->type;
        st->pos = node->pos;
        st->length = node->length;
        st->identifier_id = node->identifier_id;
        st->severity = node->severity;
        st->message_code = NULL; // No message for structural marker
        st->message = NULL;      // No message for structural marker

        /* Flatten children */
        CB_Node *child = node->child;
        while (child) {
            cb_flatten_node(child, stream);
            child = child->sibling;
        }

        /* Add TREE_UP */
        stream->tokens = (CB_SerializedToken*)safe_realloc(stream->tokens, (stream->count + 1) * sizeof(CB_SerializedToken));
        st = &stream->tokens[stream->count++];
        st->type = TREE_UP;
        st->pos = node->pos + node->length; /* End of the node */
        st->length = 0;
        st->identifier_id = 0;
        st->severity = CB_NONE;
        st->message_code = NULL;
        st->message = NULL;
    } else {
        /* Leaf node */
        stream->tokens = (CB_SerializedToken*)safe_realloc(stream->tokens, (stream->count + 1) * sizeof(CB_SerializedToken));
        CB_SerializedToken *st = &stream->tokens[stream->count++];
        st->type = node->type;
        st->pos = node->pos;
        st->length = node->length;
        st->identifier_id = node->identifier_id;
        st->severity = node->severity;
        st->message_code = node->message_code ? strdup(node->message_code) : NULL;
        st->message = node->message ? strdup(node->message) : NULL;
    }
}

CB_TokenStream* cb_flatten_tree(CB_ParseTree *tb) {
    if (!tb || !tb->root) {
        LOG("cb_flatten_tree: tb or root is NULL");
        return NULL;
    }

    CB_TokenStream *stream = (CB_TokenStream*)malloc(sizeof(CB_TokenStream));
    stream->tokens = NULL;
    stream->count = 0;

    LOG("cb_flatten_tree: starting flattening");
    cb_flatten_node(tb->root, stream);
    LOG("cb_flatten_tree: finished, count=%zu", stream->count);

    return stream;
}

CB_ParseTree* cb_reconstruct_tree(CB_TokenStream *stream) {
    if (!stream || stream->count == 0) {
        LOG("cb_reconstruct_tree: stream is NULL or empty");
        return NULL;
    }

    CB_ParseTree *tb = cb_create_token_buffer();
    LOG("cb_reconstruct_tree: starting reconstruction of %zu tokens", stream->count);
    
    for (size_t i = 0; i < stream->count; i++) {
        CB_SerializedToken *st = &stream->tokens[i];

        if (st->type == TREE_DOWN) {
            /* Next token should be the actual type of this structural node */
            i++;
            if (i >= stream->count) {
                LOG("cb_reconstruct_tree: unexpected end of stream after TREE_DOWN");
                break;
            }
            CB_SerializedToken *type_st = &stream->tokens[i];

            CB_Node node = cb_create_node(type_st->type, st->pos, st->length);
            node.identifier_id = st->identifier_id;
            node.severity = st->severity;
            node.message_code = st->message_code;
            node.message = st->message;
            
            cb_add_child_node(tb, node);
            /* Move current_parent into this new node */
            cb_set_current_parent_to_last_node(tb);
        } else if (st->type == TREE_UP) {
            /* Move current_parent up */
            cb_set_current_parent_to_grandparent(tb);
        } else {
            /* Leaf node */
            CB_Node node = cb_create_node(st->type, st->pos, st->length);
            node.identifier_id = st->identifier_id;
            node.severity = st->severity;
            node.message_code = st->message_code;
            node.message = st->message;
            
            cb_add_child_node(tb, node);
        }
    }

    LOG("cb_reconstruct_tree: finished");
    return tb;
}

void cb_free_token_stream(CB_TokenStream *stream) {
    if (!stream) return;
    for (size_t i = 0; i < stream->count; i++) {
        free(stream->tokens[i].message_code);
        free(stream->tokens[i].message);
    }
    free(stream->tokens);
    free(stream);
}

/* --- Serialization Helpers --- */

/* 
 * Simple text serialization format:
 * We'll use a very basic CSV-like format with some escaping.
 * For production, JSON or binary would be better.
 */

char* cb_serialize_delta(Delta *delta) {
    if (!delta) return NULL;

    size_t cap = 1024;
    char *buf = (char*)malloc(cap);
    size_t len = 0;
    const char *doc_id = delta->unique_document_id ? delta->unique_document_id : "";
    const char *overlay_id = delta->overlay_id ? delta->overlay_id : "";

    len += sprintf(buf + len, "2|");
    for (size_t i = 0; doc_id[i]; i++) {
        if (len + 4 > cap) {
            cap *= 2;
            buf = (char*)safe_realloc(buf, cap);
        }
        len += sprintf(buf + len, "%02x", (unsigned char)doc_id[i]);
    }
    if (len + 128 > cap) {
        cap = len + 1024;
        buf = (char*)safe_realloc(buf, cap);
    }
    len += sprintf(buf + len, "|%zu|%zu|", delta->base_version, delta->change_version);
    for (size_t i = 0; overlay_id[i]; i++) {
        if (len + 4 > cap) {
            cap *= 2;
            buf = (char*)safe_realloc(buf, cap);
        }
        len += sprintf(buf + len, "%02x", (unsigned char)overlay_id[i]);
    }
    if (len + 128 > cap) {
        cap = len + 1024;
        buf = (char*)safe_realloc(buf, cap);
    }
    len += sprintf(buf + len, "|%zu", delta->transaction_count);

    for (size_t i = 0; i < delta->transaction_count; i++) {
        Transaction *t = &delta->transactions[i];
        
        /* Ensure we have enough space for the fixed parts of the transaction */
        if (len + 256 > cap) {
            cap *= 2;
            buf = (char*)safe_realloc(buf, cap);
        }

        len += sprintf(buf + len, "|%c|%d|%d|%d|", t->type, t->pos_line, t->pos_col, t->count);
        
        if (t->content) {
            size_t content_len = strlen(t->content);
            if (len + content_len * 2 + 10 > cap) {
                cap = len + content_len * 2 + 1024;
                buf = (char*)safe_realloc(buf, cap);
            }
            for (size_t j = 0; t->content[j]; j++) {
                len += sprintf(buf + len, "%02x", (unsigned char)t->content[j]);
            }
        }
    }
    return buf;
}

static char* hex_decode(const char* hex) {
    size_t len = strlen(hex);
    if (len % 2 != 0) return NULL;
    char *out = (char*)malloc(len / 2 + 1);
    for (size_t i = 0; i < len / 2; i++) {
        unsigned int val;
        sscanf(hex + i * 2, "%02x", &val);
        out[i] = (char)val;
    }
    out[len / 2] = '\0';
    return out;
}

static char* safe_strtok(char **stringp, const char *delim) {
    char *start = *stringp;
    char *p;

    if (start == NULL) return NULL;

    if ((p = strpbrk(start, delim)) != NULL) {
        *p = '\0';
        *stringp = p + 1;
    } else {
        *stringp = NULL;
    }
    return start;
}

Delta* cb_deserialize_delta(const char *data) {
    if (!data) return NULL;
    Delta *delta = (Delta*)malloc(sizeof(Delta));
    char *copy = strdup(data);
    char *ptr = copy;
    char *token = safe_strtok(&ptr, "|");
    if (!token) { free(copy); free(delta); return NULL; }
    memset(delta, 0, sizeof(*delta));

    if (strcmp(token, "2") == 0) {
        token = safe_strtok(&ptr, "|");
        if (token && strlen(token) > 0) delta->unique_document_id = hex_decode(token);
        else delta->unique_document_id = NULL;

        token = safe_strtok(&ptr, "|");
        if (!token) { free(copy); free_delta(delta); return NULL; }
        delta->base_version = atoll(token);

        token = safe_strtok(&ptr, "|");
        if (!token) { free(copy); free_delta(delta); return NULL; }
        delta->change_version = atoll(token);

        token = safe_strtok(&ptr, "|");
        if (token && strlen(token) > 0) delta->overlay_id = hex_decode(token);
        else delta->overlay_id = NULL;

        token = safe_strtok(&ptr, "|");
        if (!token) { free(copy); free_delta(delta); return NULL; }
        delta->transaction_count = atoll(token);
    } else {
        delta->base_version = 0;
        delta->change_version = atoll(token);
        delta->unique_document_id = NULL;
        delta->overlay_id = NULL;
    
        token = safe_strtok(&ptr, "|");
        if (!token) { free(copy); free_delta(delta); return NULL; }
        delta->transaction_count = atoll(token);
    }
    
    delta->transactions = (Transaction*)malloc(delta->transaction_count * sizeof(Transaction));
    for (size_t i = 0; i < delta->transaction_count; i++) {
        Transaction *t = &delta->transactions[i];
        
        token = safe_strtok(&ptr, "|");
        if (!token) break;
        t->type = token[0];
        
        token = safe_strtok(&ptr, "|");
        if (!token) break;
        t->pos_line = atoi(token);
        
        token = safe_strtok(&ptr, "|");
        if (!token) break;
        t->pos_col = atoi(token);
        
        token = safe_strtok(&ptr, "|");
        if (!token) break;
        t->count = atoi(token);
        
        /* content is hex encoded */
        token = safe_strtok(&ptr, "|");
        if (token && strlen(token) > 0) {
            t->content = hex_decode(token);
        } else {
            t->content = NULL;
        }
    }
    free(copy);
    return delta;
}

char* cb_serialize_token_stream(CB_TokenStream *stream) {
    if (!stream) {
        char *buf = (char*)malloc(2);
        strcpy(buf, "0");
        return buf;
    }
    size_t cap = 1024 + stream->count * 128;
    char *buf = (char*)malloc(cap);
    size_t len = 0;

    len += sprintf(buf + len, "%zu", stream->count);

    for (size_t i = 0; i < stream->count; i++) {
        CB_SerializedToken *st = &stream->tokens[i];
        
        /* Check if we need to resize the buffer for this token */
        size_t msg_len = (st->message ? strlen(st->message) : 0);
        size_t code_len = (st->message_code ? strlen(st->message_code) : 0);
        if (len + 512 + (msg_len + code_len) * 2 > cap) {
            cap = len + 1024 + (msg_len + code_len) * 2;
            buf = (char*)safe_realloc(buf, cap);
        }

        len += sprintf(buf + len, "|%d|%zu|%zu|%d|%c|", (int)st->type, st->pos, st->length, st->identifier_id, (char)st->severity);
        
        if (st->message_code) {
            for (size_t j = 0; st->message_code[j]; j++)
                len += sprintf(buf + len, "%02x", (unsigned char)st->message_code[j]);
        }
        len += sprintf(buf + len, "|");
        if (st->message) {
            for (size_t j = 0; st->message[j]; j++)
                len += sprintf(buf + len, "%02x", (unsigned char)st->message[j]);
        }
    }
    return buf;
}

CB_TokenStream* cb_deserialize_token_stream(const char *data) {
    if (!data) return NULL;
    char *copy = strdup(data);
    char *ptr = copy;
    char *token = safe_strtok(&ptr, "|");
    if (!token) { free(copy); return NULL; }

    CB_TokenStream *stream = (CB_TokenStream*)malloc(sizeof(CB_TokenStream));
    stream->count = atoll(token);
    stream->tokens = (CB_SerializedToken*)malloc(stream->count * sizeof(CB_SerializedToken));

    for (size_t i = 0; i < stream->count; i++) {
        CB_SerializedToken *st = &stream->tokens[i];
        
        token = safe_strtok(&ptr, "|");
        if (!token) break;
        st->type = (CB_NodeType)atoi(token);

        token = safe_strtok(&ptr, "|");
        if (!token) break;
        st->pos = atoll(token);

        token = safe_strtok(&ptr, "|");
        if (!token) break;
        st->length = atoll(token);

        token = safe_strtok(&ptr, "|");
        if (!token) break;
        st->identifier_id = atoi(token);

        token = safe_strtok(&ptr, "|");
        if (!token) break;
        st->severity = (CB_Severity)token[0];

        token = safe_strtok(&ptr, "|");
        if (token && strlen(token) > 0) st->message_code = hex_decode(token);
        else st->message_code = NULL;

        token = safe_strtok(&ptr, "|");
        if (token && strlen(token) > 0) st->message = hex_decode(token);
        else st->message = NULL;
    }
    free(copy);
    return stream;
}

/* --- InitialLoad Serialization --- */

char* cb_serialize_initial_load(InitialLoad *load) {
    if (!load) return NULL;
    
    size_t utf8_len = 0;
    for (size_t i = 0; i < load->line_count; i++) {
        for (size_t j = 0; j < load->lines[i].length; j++) {
            utf8_len += utf32_utf8_length(load->lines[i].characters[j].character, load->lines[i].characters[j].codepoints);
        }
        utf8_len++; /* \n */
    }

    char *buf = (char*)malloc(128 + strlen(load->unique_document_id) * 2 + utf8_len * 2);
    size_t len = 0;
    len += sprintf(buf + len, "%zu|%zu|", load->change_version, load->line_count);
    
    /* unique_document_id as hex */
    for (size_t i = 0; load->unique_document_id[i]; i++)
        len += sprintf(buf + len, "%02x", (unsigned char)load->unique_document_id[i]);
    
    len += sprintf(buf + len, "|");

    /* Content as hex */
    for (size_t i = 0; i < load->line_count; i++) {
        for (size_t j = 0; j < load->lines[i].length; j++) {
            char u8[4];
            size_t n;
            for (size_t k = 0; k < load->lines[i].characters[j].codepoints; k++) {
                n = utf32_to_utf8_char(load->lines[i].characters[j].character[k], u8, 4);
                for (size_t m = 0; m < n; m++)
                    len += sprintf(buf + len, "%02x", (unsigned char)u8[m]);
            }
        }
        len += sprintf(buf + len, "0a"); /* '\n' */
    }

    return buf;
}

InitialLoad* cb_deserialize_initial_load(const char *data) {
    if (!data) return NULL;
    char *copy = strdup(data);
    char *ptr = copy;
    char *token = safe_strtok(&ptr, "|");
    if (!token) { free(copy); return NULL; }
    
    InitialLoad *load = (InitialLoad*)malloc(sizeof(InitialLoad));
    load->change_version = atoll(token);
    
    token = safe_strtok(&ptr, "|");
    if (!token) { free(copy); free(load); return NULL; }
    load->line_count = atoll(token);

    token = safe_strtok(&ptr, "|");
    if (token && strlen(token) > 0) load->unique_document_id = hex_decode(token);
    else load->unique_document_id = strdup("unknown");

    token = safe_strtok(&ptr, "|");
    if (token && strlen(token) > 0) {
        char *content = hex_decode(token);
        /* Convert full content string back to lines */
        load->lines = NULL;
        load->line_count = 0;
        char *line_ptr = content;
        char *next_line;
        while ((next_line = strchr(line_ptr, '\n')) != NULL) {
            *next_line = '\0';
            load->lines = (CodeBufferLine*)safe_realloc(load->lines, (load->line_count + 1) * sizeof(CodeBufferLine));
            utf8_to_line(line_ptr, &load->lines[load->line_count++]);
            line_ptr = next_line + 1;
        }
        /* Handle last line if no trailing newline */
        if (*line_ptr) {
             load->lines = (CodeBufferLine*)safe_realloc(load->lines, (load->line_count + 1) * sizeof(CodeBufferLine));
             utf8_to_line(line_ptr, &load->lines[load->line_count++]);
        }
        free(content);
    }

    free(copy);
    return load;
}
