# Pandu

Pandu is a general purpose testing framework written for C projects. 

It is provided as a single header file. 

## Building and testing

Tests require Make and a C23 compiler on a POSIX system (Linux or macOS).
The Makefile uses `-std=c23` when supported and falls back to the older
`-std=c2x` spelling otherwise.

Pandu's implementation uses POSIX APIs, including `kill()`. The Makefile adds
`-D_POSIX_C_SOURCE=200809L` to `CPPFLAGS` to expose their declarations in strict C
mode on Linux. If you override `CPPFLAGS` on the Make command line or use your
own build, include this definition in your compiler flags, or define
`_POSIX_C_SOURCE` as `200809L`
before any headers in the source file that defines `PANDU_IMPL`.

```sh
make           # Build all test executables in build/
make test      # Build and run all tests
make clean     # Remove build outputs
```

Compiler and flag variables can be overridden. Clean before changing compilers
or flags so existing executables are rebuilt:

```sh
make clean
make test CC=clang CFLAGS='-std=c23 -Wall -Wextra -O2'
```

Every `tests/*.c` file is automatically built as a separate executable and run by
`make test`. Each file must define `PANDU_IMPL` before including `pandu.h`, provide
its own `main()`, check test registration succeeds, and return a nonzero status
when tests fail (for example, `return run_all() != 0;`). Assertions remain enabled
in test builds, including when compiler flags define `NDEBUG`.

All test executables run even if one fails, and `make test` fails if any executable
fails. GitHub Actions runs the same command with GCC and Clang on Ubuntu for
pushes and pull requests.

## Local Git hooks

Enable the repository's hooks once per checkout:

```sh
git config --local core.hooksPath .githooks
```

Before each commit, the hook rebuilds and runs all tests with `make -B test`.
A build or test failure blocks the commit. It tests the current working tree,
including unstaged changes; stage the changes you intend to commit.

Before each push, the pre-push hook also rebuilds and runs all tests with
`make -B test`. A build or test failure blocks the push. It tests the current
working tree, including uncommitted changes.

