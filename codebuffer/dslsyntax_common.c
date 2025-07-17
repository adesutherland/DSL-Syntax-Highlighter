/* token_buffer.c */

#include <ctype.h>
#include "dslsyntax_common.h"

/* Initialization & Cleanup */

/* Create a new CB_ParseTree */
CB_ParseTree* cb_create_token_buffer() {
    CB_ParseTree *tb = (CB_ParseTree *)malloc(sizeof(CB_ParseTree));
    if (tb == NULL) {
        fprintf(stderr, "Failed to allocate memory for CB_ParseTree\n");
        return NULL;
    }
    tb->root = NULL;
    tb->current_parent = NULL;
    tb->last = NULL;
    return tb;
}

/* Recursive helper function to free nodes */
static void cb_free_node(CB_Node *node) { // NOLINT(misc-no-recursion)
    if (node == NULL)
        return;

    /* Free child nodes */
    CB_Node *child = node->child;
    while (child != NULL) {
        CB_Node *next_sibling = child->sibling;
        cb_free_node(child);
        child = next_sibling;
    }

    /* Free message strings */
    if (node->message_code != NULL)
        free(node->message_code);
    if (node->message != NULL)
        free(node->message);

    /* Free the node itself */
    free(node);
}

/* Create a blank CB_Node */
CB_Node cb_create_node(CB_NodeType type, size_t pos, size_t length) {
    CB_Node node;
    node.type = type;
    node.pos = pos;
    node.length = length;
    node.identifier_id = 0;
    node.severity = CB_NONE;
    node.message_code = NULL;
    node.message = NULL;
    node.parent = NULL;
    node.child = NULL;
    node.sibling = NULL;
    return node;
}

/* Free the CB_ParseTree and all its nodes */
void cb_free_token_buffer(CB_ParseTree *tb) {
    if (tb == NULL)
        return;

    cb_free_node(tb->root);
    free(tb);
}

/* Basic CB_Token Management */

/* Set the current_parent node to the 'parent' node */
void cb_set_current_parent_to_node(CB_ParseTree *tb, CB_Node *parent) {
    if (tb != NULL)
        tb->current_parent = parent;
}

/* Set the current_parent node to the root node */
void cb_set_current_parent_to_root_node(CB_ParseTree *tb) {
    if (tb != NULL)
        tb->current_parent = tb->root;
}

/* Set the current_parent node to the last node */
void cb_set_current_parent_to_last_node(CB_ParseTree *tb) {
    if (tb != NULL)
        tb->current_parent = tb->last;
}

/* Set the current_parent node to the grandparent node */
void cb_set_current_parent_to_grandparent(CB_ParseTree *tb) {
    if (tb != NULL && tb->current_parent != NULL)
        tb->current_parent = tb->current_parent->parent;
}

/* Set the current_parent node to the last child node of the current parent */
void cb_set_current_parent_to_last_child(CB_ParseTree *tb) {
    if (tb == NULL || tb->current_parent == NULL || tb->current_parent->child == NULL)
        return;

    CB_Node *child = tb->current_parent->child;
    while (child->sibling != NULL)
        child = child->sibling;

    tb->current_parent = child;
}

/* Helper function to copy a CB_Node */
static CB_Node* cb_copy_node(const CB_Node *node) {
    CB_Node *new_node = (CB_Node *)malloc(sizeof(CB_Node));
    if (new_node == NULL) {
        fprintf(stderr, "PANIC: Failed to allocate memory for CB_Node\n");
        exit(1);
    }

    /* Copy simple data types */
    new_node->type = node->type;
    new_node->pos = node->pos;
    new_node->length = node->length;
    new_node->identifier_id = node->identifier_id;
    new_node->severity = node->severity;

    /* Copy strings */
    if (node->message_code != NULL) {
        new_node->message_code = strdup(node->message_code);
        if (new_node->message_code == NULL) {
            fprintf(stderr, "Failed to allocate memory for message_code\n");
            free(new_node);
            return NULL;
        }
    } else {
        new_node->message_code = NULL;
    }

    if (node->message != NULL) {
        new_node->message = strdup(node->message);
        if (new_node->message == NULL) {
            fprintf(stderr, "Failed to allocate memory for message\n");
            if (new_node->message_code != NULL)
                free(new_node->message_code);
            free(new_node);
            return NULL;
        }
    } else {
        new_node->message = NULL;
    }

    /* Initialize pointers */
    new_node->parent = NULL;
    new_node->child = NULL;
    new_node->sibling = NULL;

    return new_node;
}

/* Add a new node to the parse tree as a child of the current_parent node */
/* Returns the new node pointer */
void cb_add_child_node(CB_ParseTree *tb, CB_Node node) {
    if (tb == NULL)
        return;

    /* If current_parent is NULL, set the new node as root */
    if (tb->current_parent == NULL) {
        CB_Node *new_node = cb_copy_node(&node);
        if (new_node == NULL)
            return;

        tb->root = new_node;
        tb->current_parent = new_node;
        tb->last = new_node;
        return;
    }

    /* Create a copy of the node */
    CB_Node *new_node = cb_copy_node(&node);
    if (new_node == NULL)
        return;

    new_node->parent = tb->current_parent;

    /* If current_parent has no children, set as first child */
    if (tb->current_parent->child == NULL) {
        tb->current_parent->child = new_node;
    } else {
        /* Append to the end of the child list */
        CB_Node *last_child = tb->current_parent->child;
        while (last_child->sibling != NULL)
            last_child = last_child->sibling;

        last_child->sibling = new_node;
    }
    new_node->sibling = NULL;

    /* Update last node pointers */
    tb->last = new_node;
}

/* Add a new node to the parse tree as the first child of the current_parent node */
void cb_add_first_child_node(CB_ParseTree *tb, CB_Node node) {
    if (tb == NULL)
        return;

    /* If current_parent is NULL, set the new node as root */
    if (tb->current_parent == NULL) {
        CB_Node *new_node = cb_copy_node(&node);
        if (new_node == NULL)
            return;

        tb->root = new_node;
        tb->current_parent = new_node;
        tb->last = new_node;
        return;
    }

    /* Create a copy of the node */
    CB_Node *new_node = cb_copy_node(&node);
    if (new_node == NULL)
        return;

    new_node->parent = tb->current_parent;

    /* Insert as the first child */
    new_node->sibling = tb->current_parent->child;
    tb->current_parent->child = new_node;

    /* Update last node pointers */
    tb->last = new_node;
}

/* Add a new node to the parse tree as a sibling of the current_parent node */
void cb_add_sibling_node(CB_ParseTree *tb, CB_Node node) {
    if (tb == NULL)
        return;

    /* If `current_parent` is NULL, set the new node as root */
    if (tb->current_parent == NULL) {
        CB_Node *new_node = cb_copy_node(&node);
        if (new_node == NULL)
            return;

        tb->root = new_node;
        tb->current_parent = new_node;
        tb->last = new_node;
        return;
    }

    /* Can't add a sibling to root if it has no parent */
    if (tb->current_parent->parent == NULL) {
        fprintf(stderr, "Cannot add sibling to root node\n");
        exit(1);
    }

    /* Create a copy of the node */
    CB_Node *new_node = cb_copy_node(&node);
    if (new_node == NULL)
        return;

    new_node->parent = tb->current_parent->parent;

    /* Append to the sibling list */
    new_node->sibling = tb->current_parent->sibling;
    tb->current_parent->sibling = new_node;

    /* Update last and current_parent pointers */
    tb->last = new_node;
}

/* Tree Walking */

/* Recursive helper for top-down traversal */
static void cb_walk_tree_top_down_node(CB_Node *node, size_t depth, CB_WalkTreeCallback callback, void *user_data) { // NOLINT(misc-no-recursion)
    if (node == NULL)
        return;

    /* Call the callback on the current node */
    callback(node, depth, user_data);

    /* Recurse on child nodes */
    depth++;
    CB_Node *child = node->child;
    while (child != NULL) {
 //       CB_Node *next_sibling = child->sibling;
        cb_walk_tree_top_down_node(child, depth, callback, user_data);
 //       child = next_sibling;
        child = child->sibling;

    }
}

/* Top-down tree walk */
void cb_walk_tree_top_down(CB_ParseTree *tb, CB_WalkTreeCallback callback, void *user_data) {
    if (tb == NULL || callback == NULL)
        return;

    cb_walk_tree_top_down_node(tb->root, 0, callback, user_data);
}

/* Recursive helper for bottom-up traversal */
static void cb_walk_tree_bottom_up_node(CB_Node *node, size_t depth, CB_WalkTreeCallback callback, void *user_data) { // NOLINT(misc-no-recursion)
    if (node == NULL)
        return;

    /* Recurse on child nodes first */
    CB_Node *child = node->child;
    while (child != NULL) {
        //CB_Node *next_sibling = child->sibling;
        cb_walk_tree_bottom_up_node(child, depth + 1, callback, user_data);
        //child = next_sibling;
        child = child->sibling;
    }

    /* Call the callback on the current node */
    callback(node, depth, user_data);
}

/* Bottom-up tree walk */
void cb_walk_tree_bottom_up(CB_ParseTree *tb, CB_WalkTreeCallback callback, void *user_data) {
    if (tb == NULL || callback == NULL)
        return;

    cb_walk_tree_bottom_up_node(tb->root, 0, callback, user_data);
}

/* Functions to support intelligently creating the CB_ParseTree */

/* Function to ensure the CB_ParseTree tree is ordered correctly */
/* Child nodes of each parent should be ordered by position        */
/* Walker callback to order the tree */
static void cb_order_tree_node(CB_Node *node, __attribute__((unused))size_t depth, __attribute__((unused))void *user_data) {
    if (node == NULL || node->child == NULL)
        return;

    // Sort the children based on position using a simple insertion sort
    CB_Node *sorted = NULL;
    CB_Node *current = node->child;

    while (current != NULL) {
        CB_Node *next = current->sibling;
        if (sorted == NULL || sorted->pos > current->pos) {
            current->sibling = sorted;
            sorted = current;
        } else {
            CB_Node *temp = sorted;
            while (temp->sibling != NULL && temp->sibling->pos <= current->pos) {
                temp = temp->sibling;
            }
            current->sibling = temp->sibling;
            temp->sibling = current;
        }
        current = next;
    }

    if (sorted) node->child = sorted; // Should always happen

    /* Now update the node's pos and length based on its children */
    node->pos = node->child->pos;
    CB_Node *last = node->child;
    while (last->sibling != NULL) {
        last = last->sibling;
    }
    node->length = last->pos + last->length - node->pos;
}

/* Actual Function */
void cb_order_tree(CB_ParseTree *tb) {
    /* Wall the tree bottom-up to order the nodes */
    cb_walk_tree_bottom_up(tb, cb_order_tree_node, NULL);
}

/* Functions to add missing tokens using a callback */

/* Default callback function to generate/lookup missing tokens based on position and length            */
/* This function just returns a LEXER_WHITESPACE, LEXER_EOF, or LEXER_COMMENT tokens                   */
/* This would be OK for basic implementations where the lexer might have skipped non-essential tokens  */
/* This function can be used as is - or could be called by a user-defined callback function            */
/*                                                                                                     */
/* For a better implementation, users should provide their own callback function                       */
CB_Node cb_default_get_token_callback(__attribute__((unused))void *user_data, size_t pos, size_t length, char32_t* token_chars) {
    if (token_chars == NULL) {
        fprintf(stderr, "PANIC: Invalid token_chars in cb_default_get_token_callback\n");
        exit(1);
    }
    if (length == 0) {
        fprintf(stderr, "PANIC: Invalid length in cb_default_get_token_callback\n");
        exit(1);
    }
    if (*token_chars == '\0') {
        // EOL
        CB_Node node;
        node.type = LEXER_WHITESPACE;
        node.pos = pos;
        node.length = 1;
        node.identifier_id = 0;
        node.severity = CB_NONE;
        node.message_code = NULL;
        node.message = NULL;
        node.parent = NULL;
        node.child = NULL;
        node.sibling = NULL;
        return node;
    }
    if (utf32_isspace(*token_chars)) {
        // Whitespace
        // Check how many characters are whitespace
        size_t i = 0;
        while (token_chars[i] != '\0' && utf32_isspace(token_chars[i])) {
            i++;
        }
        if (i > length)
            i = length;

        CB_Node node;
        node.type = LEXER_WHITESPACE;
        node.pos = pos;
        node.length = i;
        node.identifier_id = 0;
        node.severity = CB_NONE;
        node.message_code = NULL;
        node.message = NULL;
        node.parent = NULL;
        node.child = NULL;
        node.sibling = NULL;
        return node;
    }

    // Otherwise we assume it's a comment
    // How many trailing whitespace characters are there?
    size_t i = utf32_strlen(token_chars);
    if (i > length)
        i = length;

    while (i > 0 && utf32_isspace(token_chars[i - 1])) {
        i--;
    }

    CB_Node node;
    node.type = LEXER_COMMENT;
    node.pos = pos;
    node.length = i;
    node.identifier_id = 0;
    node.severity = CB_NONE;
    node.message_code = NULL;
    node.message = NULL;
    node.parent = NULL;
    node.child = NULL;
    node.sibling = NULL;
    return node;
}

/* Structure to hold data for the callback */
typedef struct {
    CB_ParseTree *tb;
    CodeBuffer *cb;
    CB_GetTokenCallback callback;
    void *user_data;
} CB_AddMissingTokensData;

/* Tree walker callback to add missing tokens */
/* This assumes that cb_order_tree() has been called - i.e., pos_first and pos_last are set correctly */
static void cb_add_missing_tokens_node(CB_Node *node, __attribute__((unused)) size_t depth, void *user_data) {
    CB_AddMissingTokensData *data = (CB_AddMissingTokensData *)user_data;
    if (data == NULL || node == NULL) {
        fprintf(stderr, "PANIC: Invalid data for cb_add_missing_tokens_node\n");
        exit(1);
    }

    /*
     * We are called for each node - bottom up - so what we need to do is to look
     * at its sibling to see if there is a gap between the current node and the sibling. Then call
     * the callback to get the missing tokens and add them to the tree.
     */

    if (node->parent == NULL) {
        /* Root node - we need to check code prior to the first node and after the last node */

        /* First, check the code prior to the first node */
        size_t first_pos = node->pos;
        size_t gap = first_pos;
        size_t pos = 0;
        if (gap > 0) {
            /* There is a gap */
            /* Get the text for the gap */
            char32_t*value = get_code_buffer_part(data->cb, pos, gap, NULL, NULL, NULL);
            /* call the callback to get the missing token */
            CB_Node missing = data->callback(data->user_data, pos, gap, value);
            /* Set the pos */
            missing.pos = pos;
            /* Add the missing token */
            cb_set_current_parent_to_node(data->tb, node);
            cb_add_first_child_node(data->tb, missing);

            /* Loop closing the gap */
            pos = missing.pos + missing.length;
            gap = first_pos - pos;

            while (gap > 0) {
                /* Get the text for the gap */
                value = get_code_buffer_part(data->cb, pos, gap, NULL, NULL, NULL);
                /* call the callback to get the missing token */
                missing = data->callback(data->user_data, pos, gap, value);
                /* Set the pos */
                missing.pos = pos;
                /* Add the missing token */
                cb_set_current_parent_to_last_node(data->tb);
                cb_add_sibling_node(data->tb, missing);
                /* Calculate the remaining gap */
                pos = missing.pos + missing.length;
                gap = first_pos - pos;
            }

            node->pos = 0;
            node->length += first_pos;
        }

        // Now check the code after the last node
        size_t last_pos = node->pos + node->length;
        size_t last_gap = get_code_buffer_length(data->cb) - last_pos;
        while (last_gap > 0) {
            /* There is a gap */
            /* Get the text for the gap */
            char32_t*value = get_code_buffer_part(data->cb, last_pos, last_gap, NULL, NULL, NULL);
            /* call the callback to get the missing token */
            CB_Node missing = data->callback(data->user_data, last_pos, last_gap, value);
            /* Set the pos */
            missing.pos = last_pos;
            /* Add the missing token */
            cb_set_current_parent_to_node(data->tb, node);
            cb_add_child_node(data->tb, missing);

            // Calculate the remaining gap
            last_pos = missing.pos + missing.length;
            last_gap -= missing.length;
            node->length += missing.length;
        }

        return;
    }

    else if (node->sibling == NULL) {
        /* No sibling - nothing to do */
        return;
    }

    else {
        /* Normal case - fill in the gap between the current node and the sibling */
        size_t pos = node->pos + node->length;
        size_t gap = node->sibling->pos - pos;
        if (gap > 0) {
            /* There is a gap */
            /* Get the text for the gap */
            char32_t*value = get_code_buffer_part(data->cb, pos, gap, NULL, NULL, NULL);
            /* call the callback to get the missing token */
            CB_Node missing = data->callback(data->user_data, pos, gap, value);
            /* Set the pos */
            missing.pos = pos;
            /* Add the missing token */
            cb_set_current_parent_to_node(data->tb, node);
            cb_add_sibling_node(data->tb, missing);
            // Note the walker will now call this function again for the missing token to see if there is still a gap
        }
    }
}

/* Function to add missing tokens using a callback */
/* This assumes that cb_order_tree() has been called - i.e., pos_first and pos_last are set correctly */
void cb_add_missing_tokens(CB_ParseTree *tb, CodeBuffer *cb, CB_GetTokenCallback callback, void *user_data) {
    if (callback == NULL) callback = cb_default_get_token_callback;

    /* Prepare the data for the callback */
    CB_AddMissingTokensData data;
    data.cb = cb;
    data.tb = tb;
    data.callback = callback;
    data.user_data = user_data;

    /* Walk the tree and add missing tokens */
    cb_walk_tree_bottom_up(tb, cb_add_missing_tokens_node, &data);
}

/* Function to tweak the positions of tokens in the CB_ParseTree */
/*
 * Function to tweak the position of a token in the CB_ParseTree. This is useful for improving the position of
 * tokens for syntax highlighting. The way the parse tree is built may not always result in the best position for
 * tokens, the following tweaks are implemented (token moved from the parent to the child subtree):
 *
 * - Statement terminators are moved into the end of the statement's tree
 * - Open and close brackets are moved to the beginning and end of the tree. These must be balanced for the move to happen.
 *   This table shows which "brackets" are moved for which tree types:
 *   PARSE_TREE_CODEBLOCK: LEXER_LH_BLOCK, LEXER_RH_BLOCK, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK
 *   PARSE_TREE_EXPR: LEXER_LH_BLOCK, LEXER_RH_BLOCK, LEXER_LH_EXPR, LEXER_RH_EXPR
 *   PARSE_TREE_SCOPE: LEXER_LH_BLOCK, LEXER_RH_BLOCK, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK
 *   PARSE_TREE_FUNCTION: LEXER_LH_BLOCK, LEXER_RH_BLOCK, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK
 *   PARSE_TREE_STRUCTURE: LEXER_LH_BLOCK, LEXER_RH_BLOCK, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK
 *   PARSE_TREE: LEXER_LH_BLOCK, LEXER_RH_BLOCK
 *   PARSE_TREE_STATEMENT: LEXER_STATEMENT_SEPARATOR
 *
 * Note: tokens are not moved from a subtree. This is because of the way ASTs are typically structured (they
 * don't have irrelevant child nodes). And because of the way the algorithm inserts missing tokens when creating the
 * parse tree (tokens before or after all nodes already in the tree are not added to the tree - like whitespaces).
 * Therefore, in general there is no need to move tokens from a subtree to its parent.
 *
 * This optional function should be called after all tokens have been added to the CB_ParseTree, sorted
 * with cb_order_tree() and cb_add_missing_tokens() has been called.
 */

// Static Table to hold tree types and the LH and RH tokens to move
typedef struct {
    CB_NodeType tree_type;
    CB_NodeType lh_token;
    CB_NodeType rh_token;
} TweakTable;
static TweakTable tweak_table[] = { // Note excludes PARSE_TREE_STATEMENT as this is different logic
    {PARSE_TREE_CODEBLOCK, LEXER_LH_BLOCK, LEXER_RH_BLOCK},
    {PARSE_TREE_CODEBLOCK, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK},
    {PARSE_TREE_EXPR, LEXER_LH_BLOCK, LEXER_RH_BLOCK},
    {PARSE_TREE_EXPR, LEXER_LH_EXPR, LEXER_RH_EXPR},
    {PARSE_TREE_SCOPE, LEXER_LH_BLOCK, LEXER_RH_BLOCK},
    {PARSE_TREE_SCOPE, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK},
    {PARSE_TREE_FUNCTION, LEXER_LH_BLOCK, LEXER_RH_BLOCK},
    {PARSE_TREE_FUNCTION, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK},
    {PARSE_TREE_STRUCTURE, LEXER_LH_BLOCK, LEXER_RH_BLOCK},
    {PARSE_TREE_STRUCTURE, LEXER_LH_CODEBLOCK, LEXER_RH_CODEBLOCK},
    {PARSE_TREE, LEXER_LH_BLOCK, LEXER_RH_BLOCK},
    {LEXER_EOF, LEXER_EOF, LEXER_EOF} // Use EOF as a sentinel
};

// Helper function to find the predecessor (older sibling) of a node in the tree
static CB_Node* find_predecessor(CB_Node *node) {
    if (node == NULL || node->parent == NULL)
        return NULL;

    CB_Node *child = node->parent->child;
    CB_Node *last_found = NULL;
    while (child != NULL) {
        if (child == node)
            return last_found;
        last_found = child;
        child = child->sibling;
    }
    return NULL; // Should never happen
}

// Helper function to look at rhs siblings of a tree to see if the next token is a certain type (ignoring whitespace
// and comments). It searches the current subtree, and it goes up to the parent and searches there, etc.
// Returns the node if found, otherwise NULL
static CB_Node* find_rhs_token(CB_Node *node, CB_NodeType type) { // NOLINT(misc-no-recursion)
    if (node == NULL)
        return NULL;

    CB_Node *child = node->sibling;
    while (child != NULL) {
        if (child->type == type)
            return child;
        if (child->type != LEXER_WHITESPACE && child->type != LEXER_COMMENT)
            return NULL;
        child = child->sibling;
    }
    return find_rhs_token(node->parent, type);
}

// Helper function to look at lhs siblings of a tree to see if the next token is a certain type (ignoring whitespace
// and comments). It searches the current subtree, and it goes up to the parent and searches there, etc.
// Returns the node if found, otherwise NULL
static CB_Node* find_lhs_token(CB_Node *node, CB_NodeType type) { // NOLINT(misc-no-recursion)
    if (node == NULL)
        return NULL;

    CB_Node *child = find_predecessor(node);
    while (child != NULL) {
        if (child->type == type)
            return child;
        if (child->type != LEXER_WHITESPACE && child->type != LEXER_COMMENT)
            return NULL;
        child = find_predecessor(child);
    }
    return find_lhs_token(node->parent, type);
}

// Helper function to take the rhs nodes after `subtree` up to and including node `target` and demote
// them to be appended to the children of `subtree`. This function is used to move the rhs nodes between
// `subtree` and `target` to be children of `subtree`.
static void demote_rhs_nodes(CB_Node *subtree, CB_Node *target) {
    if (subtree == NULL || target == NULL)
        return;

    // Find the last node in the subtree
    CB_Node *last_child = subtree->child;
    while (last_child->sibling != NULL) {
        last_child = last_child->sibling;
    }

    // Work through the tokens from the subtree to the target
    CB_Node *parent = subtree->parent;
    CB_Node *child = subtree->sibling;
    while (child == NULL && parent != NULL) {
        // Go up the tree
        child = parent->sibling;
        parent = parent->parent;
    }

    while (child != NULL) {
        CB_Node *next_sibling = child->sibling;
        // Remove the node from its current position / sibling list
        CB_Node *predecessor = find_predecessor(child);
        if (predecessor)
            predecessor->sibling = child->sibling;
        else
            if (parent) parent->child = child->sibling;
        // Set the parent
        child->parent = subtree;
        // Add the node to the end of the subtree
        child->sibling = NULL;
        last_child->sibling = child;
        last_child = child;
        // Check if we are done and loop if not
        if (child == target)
            break;
        child = next_sibling;
        while (child == NULL && parent != NULL) {
            // Go up the tree
            child = parent->sibling;
            parent = parent->parent;
        }
    }

    if (child == NULL) {
        // PANIC - we should never reach here
        fprintf(stderr, "PANIC: Failed to find target node in demote_rhs_nodes\n");
        exit(1);
    }
}

// Helper function for demote_lhs_nodes() to find the previous token working up the tree
static CB_Node* get_prev_token_from_tree(CB_Node *node) { // NOLINT(misc-no-recursion)
    if (node == NULL)
        return NULL;

    CB_Node *child = find_predecessor(node);
    if (child)
        return child;

    return get_prev_token_from_tree(node->parent);
}

// Helper function to take the lhs nodes after and including `target` up to and excluding `subtree` and demote
// them to be prepended to the children of `subtree`. This function is used to move the lhs nodes between
// `subtree` and `target` to be children of `subtree`.
static void demote_lhs_nodes(CB_Node *subtree, CB_Node *target) {
    if (subtree == NULL || target == NULL)
        return;

    // Work through the tokens from the subtree to the target
    CB_Node *child = get_prev_token_from_tree(subtree);
    CB_Node *parent = NULL;
    if (child) parent = child->parent;
    while (child != NULL) {
        CB_Node *prev_sibling = get_prev_token_from_tree(child);
        // Remove the node from its current position / sibling list
        CB_Node *predecessor = find_predecessor(child); // Note `predecessor` can be different to `prev_sibling`
        if (predecessor)
            predecessor->sibling = child->sibling;
        else
            if (parent) parent->child = child->sibling;
        // Set the parent
        child->parent = subtree;
        // Add the node to the start of the subtree
        child->sibling = subtree->child;
        subtree->child = child;
        // Check if we are done and loop if not
        if (child == target)
            break;
        child = prev_sibling;
        if (child) parent = child->parent;
    }

    if (child == NULL) {
        // PANIC - we should never reach here
        fprintf(stderr, "PANIC: Failed to find target node in demote_rhs_nodes\n");
        exit(1);
    }
}

// Walker callback to tweak the tree
static void cb_tweak_tree_positions_node(CB_Node *node, __attribute__((unused))size_t depth, __attribute__((unused))void *user_data) {
    if (node == NULL || node->child == NULL)
        return;

    // Check if the node is in the tweak table
    for (size_t i = 0; tweak_table[i].tree_type != LEXER_EOF; i++) {
        if (tweak_table[i].tree_type == node->type) {
            // Found the tree type
            CB_Node *lhs_token = NULL;
            CB_Node *rhs_token = NULL;
            CB_Node *potential_lhs_token = NULL;
            CB_Node *potential_rhs_token = NULL;
            CB_Node *lhs_search_from = node;
            CB_Node *rhs_search_from = node;
            // We need to remove multiple "(" nodes if they are matched by multiple ")" nodes, could be none, one or more
            while (1) {
                // Search for LHS tokens
                potential_lhs_token = find_lhs_token(lhs_search_from, tweak_table[i].lh_token);
                if (potential_lhs_token == NULL)
                    break;
                // Search for RHS tokens
                potential_rhs_token = find_rhs_token(rhs_search_from, tweak_table[i].rh_token);
                if (potential_rhs_token == NULL)
                    break;
                // Match so we are moving these tokens at least
                lhs_token = potential_lhs_token;
                rhs_token = potential_rhs_token;

                // Loop again to see if we can find more matching tokens
                lhs_search_from = lhs_token;
                rhs_search_from = rhs_token;
            }
            // Check if we have a balanced pair of tokens
            if (lhs_token != NULL && rhs_token != NULL) {
                // Move the tokens
                demote_lhs_nodes(node, lhs_token);
                demote_rhs_nodes(node, rhs_token);

                /* Now update the node's pos and length based on its children */
                node->pos = node->child->pos;
                CB_Node *last = node->child;
                while (last->sibling != NULL) {
                    last = last->sibling;
                }
                node->length = last->pos + last->length - node->pos;

                // We are done - we have processed for the node type
                return;
            }
        }
    }

    // Check for statement terminators
    if (node->type == PARSE_TREE_STATEMENT) {
        // Check if there is a statement terminator
        CB_Node *terminator = find_rhs_token(node, LEXER_STATEMENT_SEPARATOR);
        if (terminator != NULL) {
            // Move the terminator to the end of the statement
            demote_rhs_nodes(node, terminator);

            /* Now update the node's pos and length based on its children */
            node->pos = node->child->pos;
            CB_Node *last = node->child;
            while (last->sibling != NULL) {
                last = last->sibling;
            }
            node->length = last->pos + last->length - node->pos;
            return;
        }
    }

    /* Update the node's pos and length based on its children
     * in case the walker has changed children nodes earlier */
    node->pos = node->child->pos;
    CB_Node *last = node->child;
    while (last->sibling != NULL) {
        last = last->sibling;
    }
    node->length = last->pos + last->length - node->pos;
}

// Actual Function
void cb_tweak_tree_positions(CB_ParseTree *tb) {
    // Walk the tree and tweak the positions
    cb_walk_tree_bottom_up(tb, cb_tweak_tree_positions_node, NULL);
}

/* Utility Functions */

/* Function to convert CB_NodeType to a string */
const char* cb_token_type_to_string(CB_NodeType type) {
    switch (type) {
        case LEXER_WHITESPACE: return "LEXER_WHITESPACE";
        case LEXER_EOF: return "LEXER_EOF";
        case LEXER_TOKEN: return "LEXER_TOKEN";
        case LEXER_UNKNOWN: return "LEXER_UNKNOWN";
        case LEXER_COMMENT: return "LEXER_COMMENT";
        case LEXER_STRING_LITERAL: return "LEXER_STRING_LITERAL";
        case LEXER_NUMBER_LITERAL: return "LEXER_NUMBER_LITERAL";
        case LEXER_KEYWORD: return "LEXER_KEYWORD";
        case LEXER_OPERATOR: return "LEXER_OPERATOR";
        case LEXER_OPERATOR_ASSIGN: return "LEXER_OPERATOR_ASSIGN";
        case LEXER_OPERATOR_ARITHMETIC: return "LEXER_OPERATOR_ARITHMETIC";
        case LEXER_OPERATOR_LOGICAL: return "LEXER_OPERATOR_LOGICAL";
        case LEXER_SEPARATOR: return "LEXER_SEPARATOR";
        case LEXER_STATEMENT_SEPARATOR: return "LEXER_STATEMENT_SEPARATOR";
        case LEXER_LH_BLOCK: return "LEXER_LH_BLOCK";
        case LEXER_RH_BLOCK: return "LEXER_RH_BLOCK";
        case LEXER_LH_CODEBLOCK: return "LEXER_LH_CODEBLOCK";
        case LEXER_RH_CODEBLOCK: return "LEXER_RH_CODEBLOCK";
        case LEXER_LH_EXPR: return "LEXER_LH_EXPR";
        case LEXER_RH_EXPR: return "LEXER_RH_EXPR";
        case LEXER_IDENTIFIER: return "LEXER_IDENTIFIER";
        case PARSE_TREE: return "PARSE_TREE";
        case PARSE_TREE_FILE: return "PARSE_TREE_FILE";
        case PARSE_TREE_CODEBLOCK: return "PARSE_TREE_CODEBLOCK";
        case PARSE_TREE_STATEMENT: return "PARSE_TREE_STATEMENT";
        case PARSE_TREE_EXPR: return "PARSE_TREE_EXPR";
        case PARSE_TREE_COMMENT: return "PARSE_TREE_COMMENT";
        case PARSE_TREE_SCOPE: return "PARSE_TREE_SCOPE";
        case PARSE_TREE_FUNCTION: return "PARSE_TREE_FUNCTION";
        case PARSE_TREE_STRUCTURE: return "PARSE_TREE_STRUCTURE";
        case SYNTAX_ERROR: return "SYNTAX_ERROR";
        case INTERNAL_ERROR: return "INTERNAL_ERROR";
        default: return "UNKNOWN_TOKEN_TYPE";
    }
}

/* Function to convert severity to a string */
const char* cb_severity_to_string(CB_Severity severity) {
    switch (severity) {
        case CB_NONE: return "NONE";
        case CB_INFORMATION: return "INFORMATION";
        case CB_WARNING: return "WARNING";
        case CB_ERROR: return "ERROR";
        default: return "UNKNOWN_SEVERITY";
    }
}

/* Helper function to escape a string */
static char* escape_string(const char *str) {
    size_t len = strlen(str);
    char *escaped = (char *)malloc(len * 2 + 1);
    if (escaped == NULL) {
        fprintf(stderr, "PANIC: Failed to allocate memory for escaped string\n");
        exit(1);
    }

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            escaped[j++] = '\\';
            escaped[j++] = 'n';
        } else if (str[i] == '\r') {
            escaped[j++] = '\\';
            escaped[j++] = 'r';
        } else if (str[i] == '\t') {
            escaped[j++] = '\\';
            escaped[j++] = 't';
        } else if (str[i] == '\0') {
            escaped[j++] = '\\';
            escaped[j++] = '0';
        } else {
            escaped[j++] = str[i];
        }
    }
    escaped[j] = '\0';
    return escaped;
}

/* Callback function for print traversal */
static void print_node(CB_Node *node, size_t depth, void *user_data) {
    int i;
    char *space = "   ";
    CodeBuffer *cb = (CodeBuffer *)user_data;

    /* Indentation based on depth */
    for (i = 0; i < depth; i++)
        printf("%s", space);

    /* Print node information */
    if (node->length) {
        char* value = NULL;
        char* escaped = NULL;
        size_t line = 0, column = 0;
        if (cb) get_code_buffer_part(cb, node->pos, node->length, &line, &column, &value);
        if (value) escaped = escape_string(value);

        printf("> %s, Pos: %d (%d, %d), Length: %d \"%s\"\n", cb_token_type_to_string(node->type), (int)node->pos,
               (int)line, (int)column, (int)node->length, escaped);
        if (value) free(value);
        if (escaped) free(escaped);
    }
    else
        printf("> %s\n", cb_token_type_to_string(node->type));

    /* If there's a message, print it */
    if (node->message != NULL) {
        for (i = 0; i < depth; i++)
            printf("%s", space);
        printf("  Message [%s]: %s\n", cb_severity_to_string(node->severity), node->message);
    }
}

/* Function to print the CB_ParseTree for debugging */
void cb_print_token_buffer(CodeBuffer *cb, CB_ParseTree *tb) {
    if (tb == NULL)
        return;

    /* Start traversal */
    cb_walk_tree_top_down(tb, print_node, cb);
}

/* Functions to validate a parse tree - are nodes on order, and are there no gaps,
 * are pointers correct, etc. */

/* First, the walker callback to validate the tree */
static void validate_node(CB_Node *node, size_t depth, void *user_data) {
    CB_Node *last = (CB_Node *)user_data;

    if (last == NULL) {
        fprintf(stderr, "PANIC: NULL last node handle in validate_node\n");
        exit(1);
    }

    if (node == NULL) {
        fprintf(stderr, "PANIC: NULL node in validate_node\n");
        exit(1);
    }

    /* Check the parent pointer */
    if (node->parent == NULL && depth != 0) {
        fprintf(stderr, "PANIC: NULL parent pointer in validate_node\n");
        exit(1);
    }

    /* Check the sibling pointers */
    if (node->sibling != NULL) {
        if (node->sibling->parent != node->parent) {
            fprintf(stderr, "PANIC: Incorrect parent pointer in validate_node\n");
            exit(1);
        }
    }

    /* Check the child pointers */
    if (node->child != NULL) {
        if (node->child->parent != node) {
            fprintf(stderr, "PANIC: Incorrect parent pointer in validate_node\n");
            exit(1);
        }
    }

    /* Check the position and length */
    if (node->pos < 0 || node->length < 0) {
        fprintf(stderr, "PANIC: Invalid position or length in validate_node\n");
        exit(1);
    }

    /* For nodes with children, the pos should be the same as the first child, and length should be the sum of the children */
    if (node->child != NULL) {
        if (node->pos != node->child->pos) {
            fprintf(stderr, "PANIC: Incorrect position in validate_node\n");
            exit(1);
        }

        CB_Node *last_child = node->child;
        while (last_child->sibling != NULL) {
            last_child = last_child->sibling;
        }

        if (node->length != last_child->pos + last_child->length - node->pos) {
            fprintf(stderr, "PANIC: Incorrect length in validate_node\n");
            exit(1);
        }
    }

    /* There should be no gap with the sibling */
    if (node->sibling != NULL) {
        if (node->pos + node->length != node->sibling->pos) {
            fprintf(stderr, "PANIC: Incorrect gap in validate_node\n");
            exit(1);
        }
    }

    /* Check the order of the nodes */
    if (last != NULL) {
        if (last->pos + last->length != node->pos) {
            fprintf(stderr, "PANIC: Incorrect order in validate_node\n");
            exit(1);
        }
    }

    /* Update the last node - only parser tokens (no children) */
    if (node->child == NULL)
        *last = *node;
}

/* The actual function to validate the tree */
void cb_validate_tree(CB_ParseTree *tb) {
    if (tb == NULL)
        return;

    CB_Node last = {0};
    cb_walk_tree_top_down(tb, validate_node, &last);
}

// Walker callback for highlight_syntax() to process the tokens and set the syntax highlighting
static void highlight_syntax_node(CB_Node *node, __attribute__((unused)) size_t depth, void *user_data) {
    CodeBuffer *cb = (CodeBuffer *)user_data;
    if (cb == NULL || node == NULL) {
        fprintf(stderr, "PANIC: Invalid data for highlight_syntax_node\n");
        exit(1);
    }

    // Get the line and column position of the token
    size_t line = 0, col = 0;
    get_code_buffer_part(cb, node->pos, node->length, &line, &col, NULL);

    if (node->child != NULL) {
        // If the node has children, we don't highlight it but rather its children (later), we just have
        // to handle tree lines
        if (cb->attributes[line - 1][col].subtree_type) return; // Already set
        // Calculate how many lines the subtree spans
        int subtree_lines = 1;
        size_t len = node->length;
        // Deduct the rest of the length of the first line
        if (len > cb->line_lengths[line - 1] - col) len -= cb->line_lengths[line - 1] - col + 1;
        else len = 0;
        // Now loop through the lines and count how many more lines the subtree spans
        size_t l = line;
        while (len) {
            subtree_lines++;
            l++;
            if (l - 1 >= cb->line_count) {
                fprintf(stderr, "PANIC: Node spans more lines than the buffer has in highlight_syntax_node (1)\n");
                exit(1);
            }
            if (len > cb->line_lengths[l - 1]) {
                len -= cb->line_lengths[l - 1] + 1; // +1 for the line break
            } else {
                len = 0; // No more lines to process
            }
        }
        cb->attributes[line - 1][col].subtree_type = node->type; // Set the subtree type
        cb->attributes[line - 1][col].subtree_lines = subtree_lines - 1; // Set the number of lines the subtree spans (-1 as the subtree_lines member is zero-based)
        return;
    }

    // Set the syntax highlighting for the token - // Convert the CB_ParseTree token type to a highlight code
    char token_type = node->type;
    char severity = node->severity;

    // We need to step through each character in the token and set the attributes and node_lines which might cover multiple lines
    int written = 0;
    // Loop through the lines and set the syntax highlighting and message number
    while (written < node->length) {
        cb->attributes[line - 1][col].token_type = token_type; // Set the token type
        cb->attributes[line - 1][col].severity = severity; // Set the severity
        cb->node_lines[line - 1][col] = node; // Set the node line to the current node

        // Increment the col (and check if we need to move to the next line)
        col++;
        if (col >= cb->line_lengths[line - 1]) {
            // Move to the next line
            line++;
            col = 0;
            if (line - 1 > cb->line_count) {
                fprintf(stderr, "PANIC: Node spans more lines than the buffer has in highlight_syntax_node (2)\n");
                exit(1);
            }
        }

        written++; // Increment the written count
    }
}

// Highlights the buffer using its parse tree
void highlight_syntax(CodeBuffer *buffer) {
    if (buffer == NULL) {
        fprintf(stderr, "PANIC: NULL buffer in highlight_syntax\n");
        exit(1);
    }
    if (buffer->parse_tree == NULL) {
        fprintf(stderr, "PANIC: NULL parse tree in highlight_syntax\n");
        exit(1);
    }

    // Store the old attributes for dirty line processing later
    CodeBufferCharAttributes **old_attributes = buffer->attributes;

    // Malloc new attributes
    buffer->attributes = (CodeBufferCharAttributes**)malloc(sizeof(CodeBufferCharAttributes*) * buffer->line_count);
    for (int i = 0; i < buffer->line_count; i++) {
        buffer->attributes[i] = (CodeBufferCharAttributes*)malloc(sizeof(CodeBufferCharAttributes) * (buffer->line_lengths[i] + 1));
        // Initialize the attributes to default values
        for (int j = 0; j < buffer->line_lengths[i]; j++) {
            buffer->attributes[i][j].token_type = 0; // Blank
            buffer->attributes[i][j].severity = CB_NONE; // Default severity
            buffer->attributes[i][j].subtree_type = 0; // Blank
            buffer->attributes[i][j].subtree_lines = 0; // Ignored as subtree_type is 0
        }
        // Set the last attribute (line break to whitespace
        buffer->attributes[i][buffer->line_lengths[i]].token_type = LEXER_WHITESPACE; // The last character is a whitespace
        buffer->attributes[i][buffer->line_lengths[i]].severity = CB_NONE; // The last character is a whitespace
        buffer->attributes[i][buffer->line_lengths[i]].subtree_type = 0; // The last character is a whitespace
        buffer->attributes[i][buffer->line_lengths[i]].subtree_lines = 0; // The last character is a whitespace
    }

    // Size and zero node_lines
    buffer->node_lines = (CB_Node***)safe_realloc(buffer->node_lines, sizeof(CB_Node**) * buffer->line_count);
    for (int i = 0; i < buffer->line_count; i++) {
        buffer->node_lines[i] = (CB_Node**)safe_realloc(buffer->node_lines[i], (buffer->line_lengths[i] + 1) * sizeof(CB_Node*));
        // Initialize the node lines to NULL
        for (int j = 0; j < buffer->line_lengths[i]; j++) {
            buffer->node_lines[i][j] = NULL; // No node for this character
        }
    }

    /* Walk through the token buffer and set the syntax highlighting */
    cb_walk_tree_top_down(buffer->parse_tree, highlight_syntax_node, buffer);

    if (old_attributes != NULL) {
        // Compare the old attributes with the new ones and mark the lines as dirty if they are different
        for (int i = 0; i < buffer->line_count; i++) {
            for (int j = 0; j < buffer->line_lengths[i]; j++) {
                if (buffer->attributes[i][j].token_type != old_attributes[i][j].token_type ||
                    buffer->attributes[i][j].severity != old_attributes[i][j].severity ||
                    buffer->attributes[i][j].subtree_type != old_attributes[i][j].subtree_type ||
                    buffer->attributes[i][j].subtree_lines != old_attributes[i][j].subtree_lines) {
                    // Mark the line as dirty
                    buffer->dirty_lines[i] = 1;
                    break; // No need to check the rest of the line
                }
            }
        }

        // Free the old attributes
        for (int i = 0; i < buffer->line_count; i++) {
            free(old_attributes[i]);
        }
        free(old_attributes);
    }
}

/*
 * Base functionality to Load the Initial Content
 * This function sets the local CodeBuffer object, after which the codeblock
 * can be used.
 * It frees the initial load after setting the code buffer.
 */
void base_load_initial_content(CodeBuffer *cb, InitialLoad *initial_load) {

    /* Set the CodeBuffer with the initial content */
    /* Set unique_document_id */
    cb->unique_document_id = initial_load->unique_document_id;

    /* Set the lines */
    cb->lines = initial_load->lines;
    cb->line_count = initial_load->line_count;
    cb->line_lengths = initial_load->line_lengths;

    /* Initialize other fields */
    cb->snapshot_number = 0;
    cb->highest_severity = CB_NONE;
    cb->dirty_lines = (char *)malloc(sizeof(char) * cb->line_count);
    if (!cb->dirty_lines) {
        perror("Failed to allocate memory for dirty_lines");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < cb->line_count; i++) {
        cb->dirty_lines[i] = 0;
    }
    cb->parse_tree = NULL;
    cb->node_lines = NULL;
    cb->attributes = NULL;
    cb->transactions = NULL;
    cb->transaction_count = 0;

    /* Free the initial load */
    free(initial_load);

    /* Parse the buffer to create the parse tree */
    base_parse_buffer(cb);
}

/*
 * Base functionality to parse the buffer and create the parse tree.
 */
void base_parse_buffer(CodeBuffer *cb) {
    if (cb->parse_tree) {
        cb_free_token_buffer(cb->parse_tree);
        cb->parse_tree = NULL;
    }

    if (cb->parser_function) {
        /* Call the parser function to create the parse tree
         * This could be an editor-specific "emergency parser" or
         * a more complex parser at the parser / server end
         */
        cb->parser_function(cb);
    }
    else {
        /* We need to create the initial emergency parse tree /
         * using the most basic approach */
        cb->parse_tree = cb_create_token_buffer();
        /* Create the root node */
        CB_Node root = cb_create_node(PARSE_TREE_FILE, 0, 0);
        cb_set_current_parent_to_root_node(cb->parse_tree);

        /* We populate it with a LEXER_TOKEN for each line */
        size_t total_length = 0;
        for (size_t i = 0; i < cb->line_count; i++) {
            /* Get the line length */
            size_t len = cb->line_lengths[i];
            total_length += len + 1; // Include the virtual newline character
            CB_Node node = cb_create_node(LEXER_TOKEN, i, len);
            cb_add_child_node(cb->parse_tree, node);
        }
        root.length = total_length;

        /* `Set up blank node_lines */
        cb->node_lines = (CB_Node***) malloc(sizeof(CB_Node**) * cb->line_count);
        if (!cb->node_lines) {
            perror("Failed to allocate memory for node_lines");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < cb->line_count; i++) {
            cb->node_lines[i] = (CB_Node**)malloc(sizeof(CB_Node*) * (cb->line_lengths[i] + 1));
        }
    }
}

/* Utility to convert transaction code to text */
const char* transaction_type_to_string(TransactionType type) {
    switch (type) {
        case TRANSACTION_ADDLINE: return "ADDLINE";
        case TRANSACTION_DELETELINE: return "DELETELINE";
        case TRANSACTION_ADDCHARS: return "ADDCHARS";
        case TRANSACTION_DELETECHARS: return "DELETECHARS";
        case TRANSACTION_JOINLINES: return "JOINLINES";
        case TRANSACTION_SPLITLINE: return "SPLITLINE";
        default: return "INVALID";
    }
}
