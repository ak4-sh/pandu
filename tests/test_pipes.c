#define PANDU_IMPL
#include "../pandu.h"
#include <assert.h>

static void large_output(void) {
  char block[4096];
  memset(block, 'x', sizeof block);
  for (size_t i = 0; i < 256; i++) {
    assert(fwrite(block, 1, sizeof block, stdout) == sizeof block);
    assert(fwrite(block, 1, sizeof block, stderr) == sizeof block);
  }
}

int main(void) {
  // Keep a stalled runner from hanging the test suite indefinitely.
  alarm(10);
  assert(initialise_children(1) == 0);

  // Verify cleanup resets stored descriptors and can safely run twice.
  int pipe_fds[2];
  assert(pipe(pipe_fds) == 0);
  running_children.stdout_fds[0].fd = pipe_fds[0];
  running_children.stderr_fds[0].fd = pipe_fds[1];
  close_child_pipes(0);
  assert(running_children.stdout_fds[0].fd == -1);
  assert(running_children.stderr_fds[0].fd == -1);
  errno = 0;
  assert(close(pipe_fds[0]) == -1 && errno == EBADF);
  errno = 0;
  assert(close(pipe_fds[1]) == -1 && errno == EBADF);
  close_child_pipes(0);

  // Capture relayed output so the regression does not print two megabytes.
  FILE *capture = tmpfile();
  assert(capture != NULL);
  fflush(stdout);
  int saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout != -1);
  assert(dup2(fileno(capture), STDOUT_FILENO) != -1);

  Test test = {.name = "large_output", .fn = large_output};
  assert(fork_and_run_process(test, 0) == 0);
  assert(running_children.stdout_fds[0].fd == -1);
  assert(running_children.stderr_fds[0].fd == -1);
  fflush(stdout);
  assert(dup2(saved_stdout, STDOUT_FILENO) != -1);
  close(saved_stdout);

  assert(fseek(capture, 0, SEEK_END) == 0);
  assert(ftell(capture) == 2L * 256 * 4096);
  fclose(capture);
  bulk_free_children_state(&running_children);
  alarm(0);
  puts("Pipe cleanup and large output checks passed");
  return 0;
}
