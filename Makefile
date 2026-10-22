CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS ?=
LDLIBS ?=

TEST_SOURCES := $(wildcard tests/*.c)
TEST_BINARIES := $(patsubst tests/%.c,build/%,$(TEST_SOURCES))

.PHONY: all test tests clean

all: $(TEST_BINARIES)

build/%: tests/%.c pandu.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -UNDEBUG $< $(LDFLAGS) $(LDLIBS) -o $@

build:
	mkdir -p $@

tests: test

test: all
	@status=0; \
	for test in $(TEST_BINARIES); do \
		echo "Running $$test"; \
		"./$$test" || status=1; \
	done; \
	exit $$status

clean:
	rm -rf build
