#define PANDU_IMPL
#include "../pandu.h"
#include <assert.h>

static void large_output(void) {
  char block[4096];
  memset(block, 'x', sizeof block);
  for (size_t i = 0; i < 256; i++) {
    memset(block, 'o', sizeof block);
    assert(fwrite(block, 1, sizeof block, stdout) == sizeof block);
    memset(block, 'e', sizeof block);
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
  FILE *capture_err = tmpfile();
  assert(capture != NULL);
  assert(capture_err != NULL);
  fflush(NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  int saved_stderr = dup(STDERR_FILENO);
  assert(saved_stdout != -1);
  assert(saved_stderr != -1);
  assert(dup2(fileno(capture), STDOUT_FILENO) != -1);
  assert(dup2(fileno(capture_err), STDERR_FILENO) != -1);

  Test test = {.name = "large_output", .fn = large_output};
  assert(fork_and_run_process(test, 0) == 0);
  assert(running_children.stdout_fds[0].fd == -1);
  assert(running_children.stderr_fds[0].fd == -1);
  // The relay itself must flush: inspect sizes before flushing in the test.
  assert(fseek(capture, 0, SEEK_END) == 0);
  assert(ftell(capture) == 256L * 4096);
  assert(fseek(capture_err, 0, SEEK_END) == 0);
  assert(ftell(capture_err) == 256L * 4096);
  assert(dup2(saved_stderr, STDERR_FILENO) != -1);
  assert(dup2(saved_stdout, STDOUT_FILENO) != -1);
  close(saved_stdout);
  close(saved_stderr);

  rewind(capture);
  rewind(capture_err);
  for (size_t i = 0; i < 256 * 4096; i++) {
    assert(fgetc(capture) == 'o');
    assert(fgetc(capture_err) == 'e');
  }
  assert(fgetc(capture) == EOF);
  assert(fgetc(capture_err) == EOF);
  fclose(capture);
  fclose(capture_err);
  bulk_free_children_state(&running_children);
  alarm(0);
  puts("\nPipe cleanup and large output checks passed\n");
  return 0;
}
