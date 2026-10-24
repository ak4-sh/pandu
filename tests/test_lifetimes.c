#define PANDU_IMPL
#include "../pandu.h"
#include <assert.h>

static void passing_test(void) {}

static void check_stored_name(const char *expected) {
  Test test = peek(&s);
  assert(test.fn == passing_test);
  assert(test.name != NULL);
  // Include the null terminator so a copy of only strlen(name) bytes fails.
  assert(memcmp(test.name, expected, strlen(expected) + 1) == 0);
}

static void register_stack_name(void) {
  char name[] = "stack-backed test name";
  assert(register_test(passing_test, name));
  assert(peek(&s).name != name);

  // Registration must preserve the original name even if the caller edits it.
  memset(name, 'x', sizeof(name) - 1);
  check_stored_name("stack-backed test name");
}

static void check_heap_name(const char *expected) {
  size_t size = strlen(expected) + 1;
  char *name = malloc(size);
  assert(name != NULL);
  memcpy(name, expected, size);

  assert(register_test(passing_test, name));
  assert(peek(&s).name != name);
  memset(name, 'x', size);
  free(name);

  // The framework's copy must survive the caller releasing its allocation.
  check_stored_name(expected);
}

int main(void) {
  register_stack_name();
  check_stored_name("stack-backed test name");
  check_heap_name("heap-backed test name");
  check_heap_name("");

  // Run after the source buffers' lifetimes have ended, including reporting
  // and freeing each owned name in the parent.
  int failures = run_all();
  assert(failures == 0);
  assert(get_size(&s) == 0);
  assert(peek(&s).name == NULL);
  return failures != 0;
}
