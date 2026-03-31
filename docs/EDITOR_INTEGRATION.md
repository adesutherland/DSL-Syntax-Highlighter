# Editor Integration Guide

This guide explains how to integrate a text editor with the DSL Syntax Highlighter library.

## 1. Initialization
First, initialize the editor library and the communication channel.

```c
#include "dslsyntax_editor.h"
#include "serialization.h"

// Initialize threading and events
editor_init();

// Connect to the parser server
CommunicationFunctions *comm = create_socket_communication_functions("127.0.0.1", 8080);

// Create the local CodeBuffer
CodeBuffer *cb = create_code_buffer(comm, NULL);
```

## 2. Loading a Document
When a file is opened, perform an initial load. This is asynchronous.

```c
char *content = ...; // Full file content in UTF-8
InitialLoad *initial = create_initial_load("file_path", content);
load_initial_content(cb, initial);
```

## 3. Handling User Edits
For every keystroke or edit, apply a transaction. The library will automatically perform **Emergency Parsing** to keep the UI responsive.

```c
Transaction txn;
txn.type = TRANSACTION_ADDCHARS;
txn.pos_line = current_line;
txn.pos_col = current_col;
txn.content = "x";
editor_apply_transaction(cb, txn);

// Request a background sync with the parser
process_delta(cb);
```

## 4. Rendering
In your rendering loop, use the `CodeBufferCharacter` attributes to determine colors.

```c
for (int y = 0; y < visible_lines; y++) {
    CodeBufferLine *line = &cb->lines[y + scroll];
    for (int x = 0; x < line->length; x++) {
        CodeBufferCharacter attr = line->characters[x];
        render_char(line->text[x], attr.token_type, attr.severity);
    }
}
```

## 5. Handling Parser Updates
The library signals `parse_complete_event` when a new tree arrives from the server. Your editor should listen for this to trigger a re-render.

```c
if (check_parse_complete_event() == 1) {
    reset_parse_complete_event();
    // Trigger UI refresh
}
```
