#define _POSIX_C_SOURCE 200809L
#define PANDU_IMPL
#include "../pandu.h"
#include <assert.h>
#include <signal.h>

static volatile sig_atomic_t interrupts = 0;

static void handle_alarm(int signo) {
  (void)signo;
  interrupts++;
}

static void passing_test(void) { sleep(2); }

static void failing_test(void) {
  sleep(2);
  _exit(7);
}

static void check_interrupted_wait(test_fn fn, int expected) {
  interrupts = 0;
  assert(initialise_children(1) == 0);
  alarm(1);
  Test test = {.name = "interrupted_wait", .fn = fn};
  int result = fork_and_run_process(test, 0);
  alarm(0);
  bulk_free_children_state(&running_children);

  assert(interrupts > 0);
  assert(result == expected);

  // The runner must collect the child, including after an interrupted wait.
  int status;
  assert(waitpid(-1, &status, WNOHANG) == -1);
  assert(errno == ECHILD);
}

int main(void) {
  struct sigaction action = {0};
  action.sa_handler = handle_alarm;
  assert(sigemptyset(&action.sa_mask) == 0);
  // Leave SA_RESTART disabled so the signal interrupts waitpid.
  assert(sigaction(SIGALRM, &action, NULL) == 0);

  check_interrupted_wait(passing_test, 0);
  check_interrupted_wait(failing_test, 7);
  printf("\n2 waitpid tests run, 0 failed\n");
  return 0;
}
