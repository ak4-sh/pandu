# Pandu

Pandu is a single-header testing framework for C projects on POSIX systems
(Linux or macOS). Each test runs in a separate child process.

## Quick start

Copy `pandu.h` into your project. Define `PANDU_IMPL` in exactly one source file
per executable. Save this example as `example.c` alongside the header:

```c
#define PANDU_IMPL
#include "pandu.h"
#include <assert.h>

static void test_addition(void) {
  assert(2 + 2 == 4);
}

int main(void) {
  if (!register_test(test_addition, "addition")) {
    return 1;
  }
  return run_all() != 0;
}
```

Build and run with a C23 compiler:

```sh
cc -std=c23 -D_POSIX_C_SOURCE=200809L -Wall -Wextra example.c -o example
./example
```

Use `-std=c2x` if your compiler uses the older spelling for C23. Keep assertions
enabled when using `assert()` for test checks.

Tests run sequentially in reverse registration order. Their stdout and stderr
are forwarded, and Pandu prints a pass/fail result and a final summary.
`run_all()` returns the number of failed tests, or `-1` if initialization fails.
Timeout enforcement is not yet implemented; a hanging test can block the suite.

## Building and testing

Working on Pandu itself requires Make and a C23 compiler. The Makefile selects
`-std=c23` or falls back to `-std=c2x` automatically.

The Makefile also adds `-D_POSIX_C_SOURCE=200809L` to `CPPFLAGS` for POSIX API
declarations. Preserve this definition if you override `CPPFLAGS`.

```sh
make           # Build all test executables in build/
make test      # Build and run all tests
make clean     # Remove build outputs
```

Compiler and flag variables can be overridden. Clean before changing compilers
or flags so existing executables are rebuilt:

```sh
make clean
make test CC=clang
```

Every `tests/*.c` file is built as a separate executable with its own `main()`.
New suites using Pandu should follow the quick-start pattern, checking registration
and returning a nonzero status on failure. The Makefile keeps assertions enabled,
even when compiler flags define `NDEBUG`.

All test executables run even if one fails, and `make test` fails if any executable
fails. GitHub Actions runs the same command with GCC and Clang on Ubuntu for
pushes and pull requests.

## Local Git hooks

Enable the repository's hooks once per checkout:

```sh
git config --local core.hooksPath .githooks
```

The pre-commit and pre-push hooks run `make -B test`; a build or test failure
blocks the operation. Both test the current working tree, including unstaged
and uncommitted changes.

