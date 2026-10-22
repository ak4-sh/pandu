#define PANDU_IMPL
#include "../pandu.h"
#include <assert.h>
#include <string.h>

void dummy_test(void) {}

void check_pop_order(void) {
  Test t1 = {.fn = dummy_test, .name = "test1"};
  Test t2 = {.fn = dummy_test, .name = "test2"};
  Test t3 = {.fn = dummy_test, .name = "test3"};

  assert(push(t1));
  assert(push(t2));
  assert(push(t3));

  assert(strcmp(pop().name, "test3") == 0);
  assert(strcmp(pop().name, "test2") == 0);
  assert(strcmp(pop().name, "test1") == 0);
}

void check_null_name_push(void) {
  Test t = {.fn = dummy_test, .name = NULL};
  assert(push(t) == false);
}

int main(void) {
  if (!register_test(check_pop_order, "check_pop_order")) {
    return 1;
  }

  if (!register_test(check_null_name_push, "check_null_name_push")) {
    return 1;
  }

  return run_all() != 0;
}
