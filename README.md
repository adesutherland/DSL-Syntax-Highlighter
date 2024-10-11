# Simple DSL Syntax Highlighter (SDSLH) Protocol

## Overview

The **Simple DSL Syntax Highlighter (SDSLH)** protocol is a lightweight, text-based communication protocol inspired by HTTP, designed to provide syntax highlighting for domain-specific languages (DSLs) across multiple editors. It operates by running a language-specific highlighter process that communicates with editors via standard input/output streams, enabling real-time syntax highlighting through delta updates.

## Project Scope

- **Protocol Definition**: Establish a simple, text-based protocol for communication between editors and the syntax highlighter process.
- **Highlighter Implementations**:
    - **Go/ANTLR**: A highlighter process using Go and ANTLR for parsing.
    - **C/Lemon**: A highlighter process using C and the Lemon parser generator.
- **Editor Integrations**:
    - **JetBrains Plugin**: Develop a plugin to integrate the highlighter with JetBrains IDEs.
    - **LSP Wrapper**: Create a Language Server Protocol (LSP) wrapper to support editors like Visual Studio Code.
    - **Direct Protocol Support**: Facilitate direct integration with other editors that can implement the SDSLH protocol.

## Motivation

Existing syntax highlighting solutions often require custom implementations for each editor and may not cater well to DSLs. SDSLH aims to provide a generic, efficient way to deliver high-quality syntax highlighting by leveraging existing parsers and lexers. By using a simple, text-based protocol inspired by HTTP, we reduce complexity and make it easier to implement the protocol in languages like C.

## Protocol Description

### Communication Model

- **Process-Based**: The syntax highlighter runs as a separate process, communicating with the editor via standard input/output streams.
- **Text-Based Messages**: All messages are plain text, inspired by HTTP formatting, to simplify parsing in languages like C.
- **Asynchronous Interaction**: Editors send requests and receive responses without blocking the user interface, ensuring a smooth editing experience.

### Data Synchronization

- **Shared Buffer Concept**: The editor and highlighter maintain synchronized copies of the file buffer.
- **Delta Changes**: Editors send incremental updates (delta changes) to the highlighter to reflect user edits.

### Highlighting Requests

- **Range-Based Queries**: Editors can request highlighted code for specific ranges within the document.
- **Emergency Highlighting**: While awaiting the highlighter's response, the editor applies simple, immediate syntax highlighting rules.

### Response Handling

- **Async Updates**: Upon receiving the highlighter's response, the editor updates the syntax highlighting accordingly.
- **Missed Changes Reapplication**: Any edits made during the waiting period are reapplied to ensure consistency.

## High-Level Workflow

1. **Initialization**: The editor launches the highlighter process and establishes communication channels.
2. **User Edits**: As the user types, the editor sends delta changes to the highlighter.
3. **Emergency Highlighting**: The editor applies basic syntax highlighting rules immediately.
4. **Highlighting Request**: The editor requests updated highlighting data for the affected range.
5. **Highlighter Processing**: The highlighter processes the changes and generates the highlighting data.
6. **Response Handling**: The editor receives the data, updates the display, and reapplies any missed changes.
7. **Loop**: Steps 2-6 repeat as the user continues editing.

## JetBrains Plugin Approach

### Challenges

- **Custom Framework**: JetBrains IDEs use a proprietary framework for syntax highlighting and parsing (PSI and AST).
- **Asynchronous Processing**: Ensuring the UI remains responsive while integrating with the highlighter process.

### Solutions

- **AST Serialization**: Use markers in the highlighter's output to define the AST, enabling the plugin to reconstruct JetBrains' PSI structures.
- **Async Communication**: Implement the plugin to handle asynchronous responses without blocking the editor.
- **Text-Based Protocol Handling**: Parse the pure text messages inspired by HTTP within the plugin to extract necessary information.

### Implementation Steps

1. **Plugin Skeleton**: Create a basic JetBrains plugin that can load and communicate with the highlighter process.
2. **Protocol Integration**: Implement the SDSLH protocol within the plugin to send and receive text-based messages.
3. **AST Mapping**: Define how the serialized AST from the highlighter maps to PSI elements in JetBrains.
4. **UI Updates**: Ensure that syntax highlighting updates are applied smoothly without freezing the editor.

## LSP Wrapper for Other Editors

### Purpose

- **Broader Compatibility**: Allow editors that support LSP to use the syntax highlighter without implementing the SDSLH protocol directly.

### Implementation

- **Protocol Translation**: Develop a wrapper that translates between LSP messages and the SDSLH text-based protocol.
- **State Management**: Maintain synchronization between the LSP client's expectations and the highlighter process.
- **Text Message Handling**: Ensure that the wrapper can parse and generate the text-based messages appropriately.

## Highlighter Implementations

### Go/ANTLR Highlighter

- **Language Parsing**: Use ANTLR grammars to parse the DSL.
- **Performance**: Leverage Go's concurrency features for efficient processing.
- **Text-Based Output**: Generate highlighting data and AST structures as plain text messages inspired by HTTP.

### C/Lemon Highlighter

- **Lightweight Parsing**: Utilize the Lemon parser generator for a minimalistic parser.
- **Simplified Parsing**: The pure text protocol simplifies message handling in C, avoiding complex JSON parsing.
- **Protocol Compliance**: Ensure that the highlighter communicates using the SDSLH text-based protocol.

## Protocol Specification

### Message Format

The SDSLH protocol uses a simple, text-based message format inspired by HTTP. Each message consists of a header section and an optional body, separated by a blank line.

#### General Structure

```
<Command> <Resource> SDSLH/1.0
Header1: Value1
Header2: Value2

[Optional Body]
```

- **Command**: The action to be performed (e.g., `DELTA`, `HIGHLIGHT`, `ERROR`).
- **Resource**: The target of the command, such as a file or range.
- **Headers**: Key-value pairs providing additional information.
- **Body**: Optional content, such as the actual code changes or highlighting data.

#### Example Messages

- **Delta Change Request**:

  ```
  DELTA /file1.sdslh SDSLH/1.0
  Content-Length: 42

  [Delta change data]
  ```

- **Highlighting Response**:

  ```
  HIGHLIGHT /file1.sdslh SDSLH/1.0
  Content-Length: 128

  [Highlighting data]
  ```

### Commands

- **DELTA**: Informs the highlighter of text insertions, deletions, or replacements.
- **HIGHLIGHT**: Requests or provides highlighting data for a specific range.
- **ERROR**: Communicates any issues encountered during processing.
- **INIT**: Initializes the session or provides initial synchronization data.
- **PING**: Keeps the connection alive or checks if the highlighter is responsive.

### Headers

- **Content-Length**: Specifies the length of the body in bytes.
- **Range**: Specifies the text range affected, e.g., `Range: 10-20`.
- **Timestamp**: A timestamp to synchronize messages, e.g., `Timestamp: 1618033988`.
- **Content-Type**: Describes the type of content in the body, e.g., `Content-Type: text/plain`.

### Data Structures

- **Positions and Ranges**: Text positions are defined by line and character offsets, or by absolute character positions.
- **Highlight Tokens**: Include the type of token (e.g., keyword, identifier) and its position within the text.
- **AST Markers**: Special markers within the body to indicate the structure of the AST.

### Sample Communication Flow

- **Editor Sends Delta Change**:

  ```
  DELTA /example.dsl SDSLH/1.0
  Content-Length: 58
  Range: 100-105
  Content-Type: text/plain

  Added text content here
  ```

- **Highlighter Responds with Acknowledgment**:

  ```
  ACK /example.dsl SDSLH/1.0
  Timestamp: 1618033990

  Change applied successfully
  ```

- **Editor Requests Highlighting**:

  ```
  HIGHLIGHT /example.dsl SDSLH/1.0
  Range: 90-110

  [Empty body]
  ```

- **Highlighter Provides Highlighting Data**:

  ```
  HIGHLIGHT /example.dsl SDSLH/1.0
  Content-Length: 200
  Content-Type: text/plain

  [Highlighting data in the specified format]
  ```

### Error Handling

- **Error Messages**: Use the `ERROR` command with a descriptive message in the body.

  ```
  ERROR /example.dsl SDSLH/1.0
  Content-Length: 45

  Syntax error at line 10, unexpected token
  ```

## Error Handling and Robustness

- **Graceful Degradation**: If the highlighter process fails or becomes unresponsive, the editor continues using emergency highlighting.
- **Timeouts and Retries**: Implement timeouts for communication with the highlighter process and define retry policies for transient issues.
- **User Notifications**: Inform users of any persistent issues that may affect syntax highlighting.

## Performance Considerations

- **Incremental Processing**: The highlighter processes only the affected portions of the text to improve performance.
- **Asynchronous Communication**: Utilize non-blocking I/O and asynchronous programming models to prevent blocking the editor UI.
- **Efficient Parsing**: The text-based protocol reduces overhead in parsing messages, especially in languages like C.

## Testing and Validation

- **Automated Tests**: Create test cases covering various editing scenarios and language constructs.
- **Cross-Editor Consistency**: Verify that syntax highlighting behaves consistently across different editor integrations.
- **Performance Benchmarks**: Measure latency and resource usage to identify and address bottlenecks.

## Future Enhancements

- **Extended Editor Support**: Implement direct support for additional editors that can adopt the SDSLH protocol.
- **Feature Expansion**: Incorporate additional language features like code completion, linting, and refactoring support.
- **Community Collaboration**: Open-source the project to encourage contributions and broaden adoption.

## Contributing

We welcome contributions from the community. Please refer to the `CONTRIBUTING.md` file for guidelines on how to get involved.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Contact

For questions, suggestions, or feedback, please open an issue on the project's GitHub repository or contact us directly at [email address].
