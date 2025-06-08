## **Syntax Highlighting Protocol Overview**

### **1. Introduction**

The **Syntax Highlighting Protocol** is designed to enable seamless and efficient communication between an editor and a parser. It ensures that as users interact with the code (e.g., typing, deleting), syntax highlighting is updated in real-time with minimal latency and resource usage. The protocol accommodates advanced features such as syntax tree integration and error reporting, enhancing the overall editing experience.

### **2. Core Components**

#### **2.1. Editor End**

- **Function:** Captures user interactions (e.g., key presses) and manages the local `CodeBuffer` and `CB_ParseTree`.
- **Responsibilities:**
    - Applies transactions based on user inputs to update the `CodeBuffer`.
    - Performs emergency parsing on the `CB_ParseTree` for immediate syntax highlighting.
    - Creates snapshots of the `CodeBuffer` to detect changes.
    - Generates and sends deltas (sets of transactions) to the Parser End.
    - Integrates updates received from the Parser End, ensuring consistency.

#### **2.2. Parser End**

- **Function:** Receives updates from the Editor End, parses the code, and updates the `CB_ParseTree` with accurate syntax tokens and optional syntax trees.
- **Responsibilities:**
    - Parses the entire `CodeBuffer` or deltas to generate/update the `CB_ParseTree`.
    - Optimizes responses by identifying and sending only the affected range of tokens (from the first to the last changed token).
    - Incorporates syntax tree information using special tokens (`TREE_UP`, `TREE_DOWN`).
    - Reports parsing errors and warnings using `ERROR` tokens with associated messages and severity levels.

#### **2.3. CodeBuffer**

- **Function:** Represents the current_parent state of the code in the editor.
- **Structure:** An array of strings, each representing a line of code.
- **Operations:** Supports transactions such as adding/deleting lines or characters.

#### **2.4. CB_ParseTree**

- **Function:** Holds syntax tokens corresponding to the `CodeBuffer` for syntax highlighting and optional syntax trees.
- **Structure:** An array of tokens, each containing:
    - **Line Number (`line`)**: The line in the `CodeBuffer` where the token starts.
    - **Column (`col`)**: The column in the line where the token starts.
    - **Length (`length`)**: The length of the token.
    - **Type (`type`)**: The category/type of the token (e.g., keyword, string, comment, `TREE_UP`, `TREE_DOWN`, `ERROR`).
    - **Message (`message`)**: (Optional) A pointer to a `Message` structure for error reporting.

#### **2.5. Transaction**

- **Function:** Represents an edit operation (e.g., adding/deleting lines or characters).
- **Structure:**
    - **Type (`type`)**: The kind of transaction (e.g., `ADDLINE`, `DELETECHARS`).
    - **Position (`pos_line`, `pos_col`)**: The location in the `CodeBuffer` where the transaction occurs.
    - **Content (`content`)**: The text to be inserted (for add operations).
    - **Count (`count`)**: The number of characters to delete (for delete operations).

#### **2.6. Delta**

- **Function:** A collection of transactions representing changes since the last snapshot.
- **Structure:**
    - **Change Version (`change_version`)**: An incrementing version number indicating the state of changes.
    - **Unique Document ID (`unique_document_id`)**: Identifier for the document.
    - **Transactions (`transactions`)**: An array of `Transaction` structures.
    - **Transaction Count (`transaction_count`)**: The number of transactions in the delta.

#### **2.7. ParseResult**

- **Function:** The result of parsing operations, including tokens and messages.
- **Structure:**
    - **Unique Document ID (`unique_document_id`)**: Identifier for the document.
    - **Change Version (`change_version`)**: The version number after parsing.
    - **Highest Severity (`highest_severity`)**: The most severe message level encountered.
    - **CB_Token Lines (`parse_tree`)**: An array of `TokenLine` structures representing syntax tokens.
    - **Messages (`messages`)**: An array of `Message` structures for error/warning reporting.

#### **2.8. Message**

- **Function:** Represents parser messages, such as errors or warnings.
- **Structure:**
    - **Line (`line`)**: The line number where the message applies.
    - **Start Column (`start_col`)**: The starting column of the message.
    - **Length (`length`)**: The length of the affected text.
    - **Message Code (`message_code`)**: A code identifying the type of message.
    - **Severity (`severity`)**: The severity level (`INFORMATION`, `WARNING`, `ERROR`).
    - **Message Text (`message_text`)**: A descriptive message.

#### **2.9. TokenLine**

- **Function:** Represents syntax tokens per line, aiding in features like syntax highlighting.
- **Structure:**
    - **Line Number (`line`)**: The line in the `CodeBuffer`.
    - **Tokens (`tokens`)**: An array of `CB_Token` structures for the line.

---

### **3. Protocol Workflow**

The protocol ensures efficient and synchronized communication between the Editor End and the Parser End. Below is a step-by-step workflow illustrating the protocol:

#### **3.1. Initial Loading**

1. **Editor Initialization:**
    - The Editor End initializes the `CodeBuffer` with the entire content of the file.
    - Assigns a unique document ID (`unique_document_id`) to the `CodeBuffer`.

2. **Snapshot Creation:**
    - Creates an initial snapshot of the `CodeBuffer` to serve as a reference for future changes.

3. **Sending Initial Content:**
    - The Editor sends the initial `CodeBuffer` (or `Contents`) to the Parser End for parsing.

4. **Parser Parsing:**
    - The Parser End parses the received `CodeBuffer`.
    - Generates the corresponding `CB_ParseTree`, including syntax tokens and any syntax tree information using `TREE_UP` and `TREE_DOWN` tokens.
    - Identifies and sends back the affected range of tokens (initially, the entire buffer) to the Editor End.

5. **Editor Integration:**
    - The Editor receives the `CB_ParseTree` and integrates it for syntax highlighting.
    - Displays any error messages based on the received `Message` structures.

#### **3.2. Handling User Edits**

1. **Capturing Transactions:**
    - As the user interacts with the code (e.g., typing, deleting), the Editor End captures these actions as **transactions**.
    - Examples include `TRANSACTION_ADDLINE`, `TRANSACTION_ADDCHARS`, `TRANSACTION_DELETECHARS`, etc.

2. **Applying Transactions to CodeBuffer:**
    - Each transaction is applied to the `CodeBuffer`, updating the current_parent state of the code.

3. **Emergency Parsing on CB_ParseTree:**
    - Simultaneously, transactions are applied to the `CB_ParseTree` for **emergency parsing**.
    - **Emergency Parsing:**
        - Adjusts existing tokens or deletes affected tokens based on the transaction.
        - Introduces `TREE_UP` and `TREE_DOWN` tokens to represent syntax tree changes.
        - Inserts `ERROR` tokens if parsing errors are detected during emergency parsing.
        - Updates `Message` structures with relevant messages and severity levels.

4. **Snapshot and Delta Generation:**
    - After applying transactions, the Editor creates a new snapshot of the `CodeBuffer`.
    - **Delta:** The difference between the current_parent snapshot and the previous snapshot, encapsulated as a set of transactions.
    - If no transactions have occurred since the last snapshot, no delta is generated.

5. **Sending Delta to Parser End:**
    - The Editor sends the delta to the Parser End for comprehensive parsing and token updating.

#### **3.3. Synchronization with Parser End**

1. **Receiving Delta:**
    - The Parser End receives the delta containing the set of transactions.

2. **Applying Delta to Parser's CodeBuffer:**
    - The Parser applies the transactions to its own `CodeBuffer`.
    - Ensures that the Parser's `CodeBuffer` mirrors the Editor's `CodeBuffer`.

3. **Comprehensive Parsing:**
    - The Parser re-parses the affected sections of the code based on the delta.
    - Generates updated `CB_ParseTree` entries, including new `TREE_UP` and `TREE_DOWN` tokens for syntax tree adjustments.

4. **Optimized CB_Token Update:**
    - Instead of sending the entire `CB_ParseTree`, the Parser identifies the first and last changed tokens and sends only that range to the Editor End.
    - This optimization reduces data transmission overhead and speeds up synchronization.

5. **Editor Integration of Updated Tokens:**
    - The Editor receives the updated range of tokens.
    - Integrates these tokens into its `CB_ParseTree`, replacing the corresponding range.
    - Updates syntax highlighting accordingly.
    - Incorporates any new `Message` structures for error reporting.

6. **Replaying Transactions:**
    - If the user has made additional transactions while awaiting the Parser's response, the Editor replays these transactions to ensure consistency.
    - This step ensures that the `CodeBuffer` and `CB_ParseTree` remain synchronized despite concurrent edits.

#### **3.4. Handling Bulk Updates (e.g., Pasting Large Blocks of Code)**

1. **Difference Detection Algorithm:**
    - When bulk changes occur (e.g., pasting multiple lines), the Editor employs a **difference detection algorithm** to identify added and deleted lines.
    - **Approach:**
        - **Line-by-Line Comparison:** Compares the new `CodeBuffer` with the previous snapshot on a line-by-line basis.
        - **Efficient Algorithms:** Utilizes algorithms like Myers' Diff Algorithm, optimized for speed and simplicity in C90, to detect differences rapidly.

2. **Generating Transactions:**
    - Based on the detected differences, the Editor generates a list of transactions representing the added and deleted lines.
    - Example transactions:
        - `TRANSACTION_ADDLINE` for each new line inserted.
        - `TRANSACTION_DELETELINE` for each line removed.

3. **Applying and Sending Transactions:**
    - Applies these transactions to both the `CodeBuffer` and the `CB_ParseTree` (emergency parsing).
    - Sends the delta containing these transactions to the Parser End for comprehensive parsing.

4. **Parser Processing and CB_Token Update:**
    - The Parser applies the bulk transactions, re-parses the affected sections, and sends back the optimized token updates.
    - The Editor integrates these updates, ensuring accurate syntax highlighting despite the bulk changes.

---

### **4. Special CB_Token Types and Their Roles**

To enhance the protocol's functionality, especially regarding syntax tree integration and error reporting, the protocol introduces special token types with specific behaviors:

#### **4.1. `TREE_UP` and `TREE_DOWN` Tokens**

- **Purpose:** Represent transitions within a syntax tree, enabling the Editor to understand the hierarchical structure of the code for advanced features like nested syntax highlighting or structural analysis.

- **Characteristics:**
    - **Zero-Length Tokens:** Both `TREE_UP` and `TREE_DOWN` tokens have a `length` of `0`, meaning they do not correspond to actual text in the `CodeBuffer`.
    - **Directional Indicators:**
        - **`TREE_DOWN`:** Indicates moving down one level in the syntax tree (e.g., entering a function body).
        - **`TREE_UP`:** Indicates moving up one level in the syntax tree (e.g., exiting a function body).

- **Usage Example:**
    - Given a tree structure with node `A` having children `B` and `C`, and `B` having children `X`, `Y`, `Z`, the tokens would appear as:
      ```
      A TREE_DOWN
        B TREE_DOWN
          X
          Y
          Z TREE_UP
        C TREE_UP
      ```

#### **4.2. `ERROR` CB_Token**

- **Purpose:** Represents parsing errors within the code, allowing the Editor to highlight and inform users about syntax issues.

- **Characteristics:**
    - **Zero-Length CB_Token:** The `ERROR` token has a `length` of `0`, acting as a marker at a specific location in the `CodeBuffer`.
    - **Associated Messages:** The `ERROR` token can be accompanied by a `Message` structure detailing the error, including severity and descriptive text.

- **Usage Example:**
    - Placing an `ERROR` token at line `3`, column `15` to indicate a syntax error in an expression.

---

### **5. Emergency Parsing Workflow**

**Emergency Parsing** is a crucial feature that ensures syntax highlighting remains responsive and accurate, even before the Parser End processes changes. Here's how it operates within the protocol:

1. **Immediate Feedback:**
    - As transactions are applied to the `CodeBuffer`, corresponding adjustments are made to the `CB_ParseTree` through emergency parsing.
    - This ensures that users receive instant visual feedback on syntax highlighting without waiting for the Parser End's response.

2. **CB_Token Adjustments:**
    - **Adding Characters or Lines:**
        - Extends existing tokens or inserts new tokens based on the added content.
        - Introduces `TREE_DOWN` tokens when entering new syntactic constructs.
    - **Deleting Characters or Lines:**
        - Removes or shortens existing tokens.
        - Introduces `TREE_UP` tokens when exiting syntactic constructs.

3. **Error Reporting:**
    - Detects potential syntax errors during emergency parsing.
    - Inserts `ERROR` tokens at relevant locations and populates associated `Message` structures with error details.

4. **Maintaining Consistency:**
    - While emergency parsing provides immediate feedback, it may not be as comprehensive as the Parser End's parsing.
    - Once the Parser End's response arrives, the Editor reconciles any discrepancies by integrating the updated `CB_ParseTree` and messages.

---

### **6. Protocol Steps Summary**

Here's a step-by-step summary of the protocol's operation:

1. **Initialization:**
    - Editor loads the file into `CodeBuffer` and sends it to the Parser End.
    - Parser parses the content, generates `CB_ParseTree` (including syntax tree tokens), and sends it back to the Editor.

2. **User Interaction:**
    - User types or modifies code.
    - Editor captures these actions as transactions, updating `CodeBuffer` and performing emergency parsing on `CB_ParseTree`.

3. **Snapshot and Delta:**
    - Editor takes a snapshot of the updated `CodeBuffer`.
    - Generates a delta representing the transactions since the last snapshot.
    - Sends the delta to the Parser End.

4. **Parser Processing:**
    - Parser receives the delta, applies transactions to its `CodeBuffer`, and re-parses affected sections.
    - Generates updated `CB_ParseTree` entries, including `TREE_UP`, `TREE_DOWN`, and `ERROR` tokens as necessary.
    - Sends the optimized range of updated tokens back to the Editor End.

5. **Editor Integration:**
    - Editor receives the updated tokens and integrates them into `CB_ParseTree`.
    - Updates syntax highlighting based on the new tokens and messages.
    - Replays any additional transactions that occurred during the wait, ensuring consistency.

6. **Handling Bulk Edits:**
    - Editor detects bulk changes (e.g., pasting large code blocks) using a difference detection algorithm.
    - Generates and applies corresponding transactions efficiently.
    - Sends the delta to the Parser End for comprehensive parsing and token updating.

---

### **7. Ensuring Performance and Scalability**

To maintain high performance and scalability, especially with large codebases or rapid user interactions, the protocol incorporates several optimizations:

1. **Optimized Data Structures:**
    - Use efficient data structures for `CodeBuffer` and `CB_ParseTree` to handle large files without significant performance degradation.

2. **Minimized Data Transmission:**
    - By sending only necessary deltas (first and last changed tokens), the protocol reduces network overhead and speeds up synchronization between the Editor and Parser Ends.

3. **Efficient Difference Detection:**
    - Implement fast and simple diff algorithms (e.g., Myers' Diff Algorithm) tailored for C90 to detect line-by-line changes swiftly during bulk edits.

4. **Concurrency Handling:**
    - Manage concurrent transactions and ensure that the `CodeBuffer` and `CB_ParseTree` remain consistent despite rapid or overlapping user inputs.

5. **Robust Error Handling:**
    - Incorporate mechanisms to handle synchronization mismatches, parsing errors, or unexpected state changes gracefully, preventing crashes and ensuring data integrity.

---

### **8. Special Considerations**

#### **8.1. Zero-Length Tokens (`TREE_UP`, `TREE_DOWN`, `ERROR`)**

- **Representation:**
    - These tokens have a `length` of `0`, meaning they do not correspond to actual text in the `CodeBuffer`.
    - They act as markers to denote structural or error-related information.

- **Integration:**
    - The Editor uses these tokens to adjust syntax highlighting based on the syntactic structure of the code.
    - For example, `TREE_DOWN` signals the start of a new syntactic block, prompting the Editor to adjust highlighting accordingly.

#### **8.2. Message and Severity Association**

- **Flexibility:**
    - `Message` structures can be associated with any token, providing contextual information about parsing results.
    - This allows for granular error reporting, enabling the Editor to highlight specific code segments with relevant messages.

- **Error CB_Token Usage:**
    - The `ERROR` token can be used independently to mark locations with parsing issues.
    - When an `ERROR` token is inserted, it is accompanied by a `Message` detailing the nature and severity of the error.

---

### **9. Protocol Advantages**

1. **Real-Time Responsiveness:**
    - Emergency parsing ensures that syntax highlighting remains immediate and fluid, enhancing the user experience.

2. **Efficiency:**
    - Optimized data transmission minimizes bandwidth usage and accelerates synchronization between Editor and Parser Ends.

3. **Scalability:**
    - The protocol can handle large codebases and rapid user interactions without compromising performance.

4. **Flexibility:**
    - Supports advanced features like syntax tree integration and detailed error reporting, allowing for sophisticated editing capabilities.

5. **Robustness:**
    - Incorporates mechanisms to handle synchronization mismatches and errors gracefully, maintaining data integrity and application stability.

---
