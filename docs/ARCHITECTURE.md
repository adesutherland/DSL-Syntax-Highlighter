# DSL Syntax Highlighter: Architecture and Design

## 1. Overview
The DSL Syntax Highlighter (DSLSH) is a C-based platform designed to decouple language parsing from text editing. It provides a robust synchronization layer that allows editors to receive high-fidelity, hierarchical syntax highlighting from out-of-process parsers with zero-latency visual feedback.

## 2. Core Architectural Patterns

### 2.1 Delta-Based Synchronization
Instead of sending full document buffers on every change, DSLSH uses an atomic **Transaction** model. Every edit (character addition, line deletion, etc.) is captured as a transaction. These are bundled into **Deltas** and synchronized over a network socket.

### 2.2 Double-Buffered State
Both the Editor (Client) and the Parser (Server) maintain a symmetric **CodeBuffer**.
- The Editor applies user changes immediately to its local buffer.
- The Parser applies received deltas to its mirror buffer before re-parsing.
- This ensures that both ends always agree on the document state and versioning.

Parser transports can now retain more than one document mirror in one parser
process. `InitialLoad` creates or replaces the session identified by
`unique_document_id`; normal deltas mutate only that session. Hypothesis deltas
reuse the same parser and retained language state, but parse against a scratch
copy so code-completion previews can ask "what would the syntax be if this text
were inserted?" without changing the real editor or parser buffer.

### 2.3 Emergency Parsing (Zero Latency)
To avoid the "flicker" associated with asynchronous network communication, the library implements **Emergency Parsing**.
- When an edit occurs, the library heuristically shifts and extends the existing local **CB_ParseTree** nodes.
- **Improved Heuristics**: The library now performs a single-pass scan of modified lines to instantly identify comments (`//`, `/*`), strings (`"`, `'`), and numeric literals.
- **Attribute Inheritance**: New characters intelligently inherit highlighting from their left neighbors, ensuring continuation of styles (like keywords or variables) feels seamless.
- This maintains "good enough" highlighting instantly.
- The real, perfectly accurate parse tree replaces the emergency tree once the parser server responds.
- Full details are available in **[Emergency Parsing Rules](EMERGENCY_PARSING.md)**.

### 2.4 Differential Syncing (LCS)
For bulk updates like copy-paste or external file modifications, the library includes a built-in **Longest Common Subsequence (LCS)** diffing engine. It automatically calculates the minimal set of transactions needed to sync the document, reducing network traffic.

## 3. Component Breakdown

### 3.1 Platform Library (`codebuffer/`)
The core engine, containing:
- **Data Structures**: `CodeBuffer`, `CB_ParseTree`, `CB_Node`.
- **Marshalling**: Hex-encoded text protocol for robust transmission.
- **Transport**: 
    - **STDIO (Default)**: Process-based communication using pipes. Handles child lifecycle and framing.
    - **Sockets**: Network-based communication for persistent or remote servers.
- **Threading**: Background synchronization threads and event signaling in `thread_utils.c`.
- **Logging**: A diagnostic logging system (`-d` flag) for tracing synchronization events.

### 3.2 Protocol Layer
Defined in `docs/PROTOCOL.md`. It uses a length-prefixed, text-based format. Hierarchical trees are transmitted as a flat stream of tokens using `TREE_DOWN` and `TREE_UP` markers.

### 3.3 Example Endpoints
- **Toy Editor (`toyeditor/`)**: An `ncurses` client demonstrating integration, rendering, and handling of parser events.
- **Toy Parser (`toyparser/`)**: A reference server implementing a simple language parser and utilizing the platform's tree-management utilities.

## 4. Stability and Security
- **No-Panic Policy**: Library functions return error codes and log to diagnostic files rather than calling `exit()`, ensuring host process stability.
- **Dangling Pointer Safety**: Explicit node pointer clearing (`cb_clear_node_pointers`) prevents segfaults during asynchronous tree swaps.
- **Memory Management**: Strictly tracked allocations with safe reallocation helpers.

## 5. Build and Verification
The project uses CMake and includes a comprehensive suite of automated tests via `ctest`.
- **`serialization_test`**: Verifies data marshalling.
- **`socket_test`**: Verifies IPC integrity.
- **`diff_test`**: Verifies the LCS engine.
