#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static size_t outstanding_allocations;
static bool fail_next_allocation;

static void *tracked_malloc(size_t size) {
  if (fail_next_allocation) {
    fail_next_allocation = false;
    errno = ENOMEM;
    return NULL;
  }
  void *ptr = malloc(size);
  if (ptr != NULL) {
    outstanding_allocations++;
  }
  return ptr;
}

static void tracked_free(void *ptr) {
  if (ptr != NULL) {
    assert(outstanding_allocations > 0);
    outstanding_allocations--;
  }
  free(ptr);
}

#define malloc tracked_malloc
#define free tracked_free
#define PANDU_IMPL
#include "../pandu.h"
#undef malloc
#undef free
#include "test_output.h"

void dummy_test(void) {}

static Test owned_test(const char *name) {
  size_t size = strlen(name) + 1;
  char *copy = tracked_malloc(size);
  assert(copy != NULL);
  memcpy(copy, name, size);
  return (Test){.fn = dummy_test, .name = copy};
}

void check_pop_order(void) {
  Stack stack = {0};
  Test t1 = owned_test("test1");
  Test t2 = owned_test("test2");
  Test t3 = owned_test("test3");

  assert(push(&stack, t1));
  assert(push(&stack, t2));
  assert(push(&stack, t3));

  const char *expected[] = {"test3", "test2", "test1"};
  for (size_t i = 0; i < 3; i++) {
    Test popped = pop(&stack);
    assert(strcmp(popped.name, expected[i]) == 0);
    tracked_free((void *)popped.name);
  }
  assert(get_size(&stack) == 0);
}

void check_null_name_push(void) {
  Stack stack = {0};
  Test t = {.fn = dummy_test, .name = NULL};
  OutputCapture errors = capture_output(STDERR_FILENO);
  assert(push(&stack, t) == false);
  expect_output(errors, "Test name is NULL\n");
}

void check_peek_decrement(void) {
  Stack stack = {0};
  Test t1 = owned_test("test1");
  Test t2 = owned_test("test2");
  Test t3 = owned_test("test3");

  assert(push(&stack, t1));
  assert(push(&stack, t2));
  assert(push(&stack, t3));

  assert(get_size(&stack) == 3);

  Test t = peek(&stack);
  assert((t.name == t3.name) && (t.fn == t3.fn));
  assert(get_size(&stack) == 3);

  assert(cleanStack(&stack));
}

void check_push_one(void) {
  Stack stack = {0};
  Test t = owned_test("test");
  assert(push(&stack, t) == true);
  assert(get_size(&stack) == 1);
  Test popped = pop(&stack);
  assert(popped.name == t.name);
  tracked_free((void *)popped.name);
  assert(get_size(&stack) == 0);
}

void check_stack_sentinels(void) {
  Stack stack = {0};

  Test t = pop(&stack);
  assert(t.name == NULL_TEST.name && t.fn == NULL_TEST.fn);
  t = peek(&stack);
  assert(t.name == NULL_TEST.name && t.fn == NULL_TEST.fn);
  assert(get_size(&stack) == 0);

  OutputCapture errors = capture_output(STDERR_FILENO);
  t = pop(NULL);
  assert(t.name == ERROR_TEST.name && t.fn == ERROR_TEST.fn);
  t = peek(NULL);
  expect_output(errors, "Stack pointer is NULL\nStack pointer is NULL\n");
  assert(t.name == ERROR_TEST.name && t.fn == ERROR_TEST.fn);
  assert(ERROR_TEST.name != NULL_TEST.name);
}

void check_peek_null_stack(void) {
  Stack stack = {0};
  Test t = peek(&stack);
  assert((t.name == NULL) && t.fn == NULL);
}

void check_repeated_peek(void) {
  Stack stack = {0};
  Test t1 = owned_test("test1");
  Test t2 = owned_test("test2");
  Test t3 = owned_test("test3");

  assert(push(&stack, t1));
  assert(push(&stack, t2));
  assert(push(&stack, t3));

  int before_count = get_size(&stack);
  for (int i = 0; i < 10; i++) {
    Test result = peek(&stack);
    assert(result.fn == t3.fn);
    assert(result.name == t3.name);
  }

  assert(before_count == get_size(&stack));
  assert(cleanStack(&stack) == true);
  assert(stack.head == NULL);
  assert(get_size(&stack) == 0);
  assert(cleanStack(&stack));
}

void check_failed_push_ownership(void) {
  Stack stack = {0};
  Test t = owned_test("caller-owned");
  size_t before = outstanding_allocations;

  OutputCapture errors = capture_output(STDERR_FILENO);
  assert(!push(NULL, t));
  expect_output(errors, "Stack pointer is NULL\n");
  assert(outstanding_allocations == before);
  assert(strcmp(t.name, "caller-owned") == 0);

  fail_next_allocation = true;
  errors = capture_output(STDERR_FILENO);
  assert(!push(&stack, t));
  char expected[256];
  snprintf(expected, sizeof expected, "malloc: %s\n", strerror(ENOMEM));
  expect_output(errors, expected);
  assert(!fail_next_allocation);
  assert(outstanding_allocations == before);
  assert(stack.head == NULL && get_size(&stack) == 0);
  assert(strcmp(t.name, "caller-owned") == 0);
  tracked_free((void *)t.name);
}

int main(void) {
  // Check allocation balance in the parent: child exit can hide leaked names.
  test_fn checks[] = {check_stack_sentinels, check_pop_order,
                      check_null_name_push,  check_peek_decrement,
                      check_push_one,        check_peek_null_stack,
                      check_repeated_peek,   check_failed_push_ownership};
  for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    checks[i]();
    assert(outstanding_allocations == 0);
  }
  OutputCapture errors = capture_output(STDERR_FILENO);
  assert(!cleanStack(NULL));
  expect_output(errors, "Stack pointer is NULL\n");

  if (!register_test(check_stack_sentinels, "check_stack_sentinels")) {
    return 1;
  }

  if (!register_test(check_pop_order, "check_pop_order")) {
    return 1;
  }

  if (!register_test(check_null_name_push, "check_null_name_push")) {
    return 1;
  }

  if (!register_test(check_peek_decrement, "check_peek_decrement")) {
    return 1;
  }

  if (!register_test(check_push_one, "check_push_one")) {
    return 1;
  }

  if (!register_test(check_peek_null_stack, "check_peek_null_stack")) {
    return 1;
  }

  if (!register_test(check_repeated_peek, "check_repeated_peek")) {
    return 1;
  }

  if (!register_test(check_failed_push_ownership,
                     "check_failed_push_ownership")) {
    return 1;
  }

  int failures = run_all();
  assert(outstanding_allocations == 0);
  return failures != 0;
}
