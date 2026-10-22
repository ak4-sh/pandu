#define PANDU_IMPL
#include "../pandu.h"
#include <assert.h>
#include <string.h>

void dummy_test(void) {}

void check_pop_order(void) {
  Stack stack = {0};
  Test t1 = {.fn = dummy_test, .name = "test1"};
  Test t2 = {.fn = dummy_test, .name = "test2"};
  Test t3 = {.fn = dummy_test, .name = "test3"};

  assert(push(&stack, t1));
  assert(push(&stack, t2));
  assert(push(&stack, t3));

  assert(strcmp(pop(&stack).name, "test3") == 0);
  assert(strcmp(pop(&stack).name, "test2") == 0);
  assert(strcmp(pop(&stack).name, "test1") == 0);
}

void check_null_name_push(void) {
  Stack stack = {0};
  Test t = {.fn = dummy_test, .name = NULL};
  assert(push(&stack, t) == false);
}

void check_peek_decrement(void) {
  Stack stack = {0};
  Test t1 = {.fn = dummy_test, .name = "test1"};
  Test t2 = {.fn = dummy_test, .name = "test2"};
  Test t3 = {.fn = dummy_test, .name = "test3"};

  assert(push(&stack, t1));
  assert(push(&stack, t2));
  assert(push(&stack, t3));

  assert(getSize(&stack) == 3);

  Test t = peek(&stack);
  assert((t.name == t3.name) && (t.fn == t3.fn));
  assert(getSize(&stack) == 3);

  pop(&stack);
  pop(&stack);
  pop(&stack);
}

void check_push_one(void) {
  Stack stack = {0};
  Test t = {.fn = dummy_test, .name = "test"};
  assert(push(&stack, t) == true);
  assert(getSize(&stack) == 1);
  pop(&stack);
}

void check_stack_sentinels(void) {
  Stack stack = {0};

  Test t = pop(&stack);
  assert(t.name == NULL_TEST.name && t.fn == NULL_TEST.fn);
  t = peek(&stack);
  assert(t.name == NULL_TEST.name && t.fn == NULL_TEST.fn);
  assert(getSize(&stack) == 0);

  t = pop(NULL);
  assert(t.name == ERROR_TEST.name && t.fn == ERROR_TEST.fn);
  t = peek(NULL);
  assert(t.name == ERROR_TEST.name && t.fn == ERROR_TEST.fn);
  assert(ERROR_TEST.name != NULL_TEST.name);
}

int main(void) {
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

  return run_all() != 0;
}
