# Parser Tester (`parser_tester`)

The `parser_tester` is a universal REPL (Read-Eval-Print Loop) and script-driven test harness designed to facilitate the rapid development, debugging, and verification of DSL-Syntax-Highlighter (DSLSH) compatible parsers.

It eliminates the need to manually drive a full text editor (like THE or `toyeditor`) to see how your parser behaves under load. Instead, it allows automated scripts (or LLMs) to precisely inject text, delete characters, and instantly dump the resulting AST (Abstract Syntax Tree) and parser state.

## Why Use `parser_tester`?
1. **Isolated Testing**: It tests *only* the parser's communication and syntax tree generation, isolating it from editor-specific rendering bugs or threading issues.
2. **Crash Resilience Verification**: It allows you to intentionally crash a parser (e.g., via a known bug) and observe whether the editor's auto-recovery mechanisms successfully resurrect the parser or transition into a `SUSPENDED` state.
3. **Automated Regression Testing**: By feeding it a predefined script of `INSERT` and `DELETE` commands, you can instantly verify that a parser builds the correct tree for complex, iterative edits without requiring human interaction.

## Building
The `parser_tester` is built automatically alongside the DSLSH platform:
```bash
cmake -B cmake-build-debug
cmake --build cmake-build-debug --target parser_tester
```
The executable will be located at `cmake-build-debug/tools/parser_tester`.

## Usage
You can run the tester interactively:
```bash
./cmake-build-debug/tools/parser_tester
```
Or, more commonly, feed it a script of commands via `stdin`:
```bash
./cmake-build-debug/tools/parser_tester < test_script.txt
```

## Available Commands

The REPL understands the following plain-text commands (one per line):

### `INIT <parser_cmd> <source_file>`
Initializes the tester. It launches the parser server specified by `<parser_cmd>`, loads the contents of `<source_file>`, and sends an `InitialLoad` payload.
* **Note**: `parser_tester` automatically disables the production "auto-relaunch" behavior. If your parser crashes during testing, the status will immediately reflect the crash rather than hiding it behind silent restarts.
* **Example**: `INIT ./cmake-build-debug/toyparser/tp test.toy`
* **Example with args**: `INIT "./cmake-build-debug/bin/rxas -d --parser" test.rxas`

### `INSERT <line> <col> <text...>`
Simulates the user typing text into the buffer at the specified 0-based line and column index. It immediately triggers a delta update to the parser.
* **Example**: `INSERT 0 0 // A comment`

### `DELETE <line> <col> <count>`
Simulates the user deleting `<count>` characters starting from the 0-based line and column index. It immediately triggers a delta update to the parser.
* **Example**: `DELETE 0 0 3`

### `DUMP_AST`
Prints a human-readable, recursive view of the current Abstract Syntax Tree (AST) as received from the parser.

### `STATUS`
Prints the current health of the out-of-process parser server.
Possible values:
* `STATUS: NOT_LOADED` (No parser initialized)
* `STATUS: ACTIVE` (Parser is running and responding)
* `STATUS: CRASHED` (Parser process died unexpectedly or closed the pipe)
* `STATUS: SUSPENDED` (Parser crashed too many times and was disabled)

### `QUIT`
Exits the test harness cleanly.

## Recommended Test Suite for a New Parser

When developing a new parser integration (e.g., `rxas`, `rxc`), you should execute the following test scenarios using `parser_tester`:

1. **Initial Load Test**:
   * **Action**: `INIT` a large, complex file. `STATUS`. `DUMP_AST`.
   * **Expected**: `STATUS: ACTIVE`. The AST matches the entire file correctly.

2. **Single Character Edit (Typing)**:
   * **Action**: `INSERT` a single character in the middle of a keyword (e.g., breaking the syntax). `STATUS`. `DUMP_AST`.
   * **Expected**: `STATUS: ACTIVE`. The AST should shift the broken keyword to `UNKNOWN` or `ERROR` nodes.

3. **Multi-Line Edit (Pasting)**:
   * **Action**: `INSERT` a block of text containing newline characters `\n` (Note: currently `parser_tester` simulates this via multiple inserts or continuous strings if escaped).
   * **Expected**: `STATUS: ACTIVE`. The AST correctly expands.

4. **Destructive Delete**:
   * **Action**: `DELETE` a structural character like a closing brace `}` or a quote `"`. `STATUS`. `DUMP_AST`.
   * **Expected**: `STATUS: ACTIVE`. The parser should gracefully degrade (e.g., turning the rest of the file into a string literal or flagging missing braces), but it **must not crash or hang**.

5. **Crash Recovery Test (Manual)**:
   * **Action**: Temporarily introduce a `raise(SIGSEGV)` into your parser's C code when it encounters a specific string (e.g., "CRASH_ME"). `INSERT` that string. `STATUS`.
   * **Expected**: The tester should report `STATUS: CRASHED` immediately.

## Lessons Learned & Future Improvements

During the development of `parser_tester` and testing against `toyparser` and `rxas`, several key lessons were learned:

1. **Beware Double Parses & Tree Flattening**: We discovered a critical bug where `base_load_initial_content` was internally triggering a parse, and the server loop was accidentally triggering a second parse. In `rxas`, this resulted in a double-free memory corruption because the assembler's cleanup routine (`rxasclrc`) wasn't perfectly idempotent. `parser_tester` exposes these lifecycle bugs instantly.
2. **Argument Parsing Collisions**: When `parser_tester` attempted to pass `--parser` to `toyparser` (which was intended for `rxas`), `toyparser` misinterpreted the string as a port number (evaluating to port `0`). This caused `toyparser` to silently become a socket server while the editor waited indefinitely on `STDIN`. **Lesson**: Always ensure the `INIT` command's `parser_cmd` is quoted and strictly matches what the target parser expects.
3. **Timeouts are Critical**: Originally, the test harness would block forever if the parser silently died without closing `STDOUT` or if it went into an infinite loop. We added a 10-second timeout to `wait_for_parser()`. If the parser fails to return an AST within 10 seconds, `parser_tester` explicitly invokes `cb_kill_parser_process(cb)`, instantly terminating the parser process with `SIGKILL` (or `TerminateProcess` on Windows) and shutting down the pipes. This completely prevents the test harness from hanging and returns `STATUS: SUSPENDED`.
4. **Re-launching vs Strict Mode**: While production editors (like THE) use an auto-relaunch policy (up to 3 crashes), a test harness *must* fail loudly. We introduced `cb_set_auto_relaunch(cb, 0)` specifically for `parser_tester` so that developers are immediately aware of Segfaults during development.
