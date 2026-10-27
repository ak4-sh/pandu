#include <assert.h>
#include <errno.h>
#include <stdlib.h>

static size_t allocation_calls;
static size_t fail_at;
static size_t outstanding;

static void *checked_malloc(size_t size) {
  allocation_calls++;
  if (allocation_calls == fail_at) {
    errno = ENOMEM;
    return NULL;
  }
  void *ptr = malloc(size);
  if (ptr != NULL)
    outstanding++;
  return ptr;
}

static void checked_free(void *ptr) {
  if (ptr != NULL) {
    assert(outstanding > 0);
    outstanding--;
  }
  free(ptr);
}

#define malloc checked_malloc
#define free checked_free
#define PANDU_IMPL
#include "../pandu.h"
#undef malloc
#undef free
#include "test_output.h"

int main(void) {
  assert(initialise_children(3) == 0);
  const size_t total_calls = allocation_calls;
  assert(running_children.count == 3);
  for (size_t i = 0; i < running_children.count; i++) {
    PipeFd *streams[] = {&running_children.stdout_fds[i],
                         &running_children.stderr_fds[i]};
    for (size_t j = 0; j < 2; j++) {
      assert(streams[j]->fd == -1);
      assert(streams[j]->output_buffer != NULL);
      assert(streams[j]->output_used == 0);
      assert(streams[j]->output_capacity > 0);
      // Verify the advertised storage is writable, including its last byte.
      streams[j]->output_buffer[0] = 'a';
      streams[j]->output_buffer[streams[j]->output_capacity - 1] = 'z';
    }
    assert(running_children.stdin_fds[i].fd == -1);
    assert(running_children.stdin_fds[i].output_buffer == NULL);
    assert(running_children.stdin_fds[i].output_used == 0);
    assert(running_children.stdin_fds[i].output_capacity == 0);
  }
  bulk_free_children_state(&running_children);
  assert(outstanding == 0);

  // Fail each allocation in turn, including partially allocated stream arrays
  // and output buffers. A failure must leave existing global state untouched.
  for (size_t failure = 1; failure <= total_calls; failure++) {
    allocation_calls = 0;
    fail_at = failure;
    running_children.count = 99;
    OutputCapture errors = capture_output(STDERR_FILENO);
    assert(initialise_children(3) == -1);
    expect_output(
        errors, failure <= total_calls - 6
                    ? "Malloc error while trying to intiliase children state\n"
                    : "Failed to allocate child output buffers\n");
    assert(outstanding == 0);
    assert(running_children.count == 99);
    assert(running_children.stdout_fds == NULL);
    assert(running_children.stderr_fds == NULL);
  }
  running_children = (Children){0};
  puts("\nChild buffer initialization and allocation failure checks passed\n");
  return 0;
}
