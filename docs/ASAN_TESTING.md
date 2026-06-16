# DSLSH ASan and LSan Testing

Use `tools/asan-run.sh` for AddressSanitizer and LeakSanitizer runs. It keeps
build and CTest output in timestamped log directories and provides status/kill
helpers for long runs.

## Configure

Create a sanitizer build tree before using the runner:

```sh
cmake -S . -B cmake-build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-g -O1 -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' \
  -DDSLSH_BUILD_PARSERS=OFF \
  -DDSLSH_BUILD_EXAMPLES=ON \
  -DDSLSH_BUILD_TOOLS=ON \
  -DDSLSH_BUILD_TESTS=ON
```

`DSLSH_BUILD_PARSERS=OFF` avoids network-backed parser adapter dependencies and
is sufficient for the core `CodeBuffer`, stdio, and `parser_tester` lifecycle
checks. Use a full parser-enabled ASan build when validating parser adapters.

## Runner

```sh
tools/asan-run.sh --phase focused-lsan
tools/asan-run.sh --phase full --test-jobs 4
tools/asan-run.sh --phase ctest --regex '^parser_tester_reinit$' --leaks on --test-jobs 1
```

Default build directory is `cmake-build-asan`. The latest run is linked from:

```sh
cmake-build-asan/asan-logs/latest
```

Check or stop a run without hunting process IDs:

```sh
tools/asan-run.sh --status cmake-build-asan/asan-logs/latest
tools/asan-run.sh --kill cmake-build-asan/asan-logs/latest
```

## Leak Policy

Build commands and CTest phases use leak detection by default for `--phase full`
and `--phase focused-lsan`. Use `--build-leaks off` or `--leaks off` only for
short-lived diagnostics when isolating a non-leak sanitizer failure.

Focused leak triage is serialized and runs:

```text
parser_tester_reinit
ep_test
stdio_test
stdio_shutdown_test
```

LeakSanitizer cannot run while the process is being traced. If every ASan test
fails immediately with `LeakSanitizer does not work under ptrace`, rerun the
same runner command outside the traced environment rather than disabling leak
detection.

## Validation Log

2026-06-16:

* Normal Debug CTest passed: 12 tests passed and `socket_test` skipped, log
  `/tmp/dslsh-debug-ctest.F79C5T.log`.
* Focused ASan/LSan passed with build and test leak detection enabled, log
  directory `cmake-build-asan/asan-logs/20260616-172720-focused-lsan`.
