#ifndef PANDU_H
#define PANDU_H

#include <stdbool.h>
typedef void (*test_fn)(void);

typedef struct {
  const char *name;
  test_fn fn;
} Test;

bool register_test(test_fn fn, const char *name);
int run_all(void);
#endif // PANDU HEADER END

#if defined(PANDU_IMPL) && !defined(PANDU_IMPL_INCLUDED)
#define PANDU_IMPL_INCLUDED

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct StackNode {
  Test test;
  struct StackNode *next;
} StackNode;

typedef struct {
  StackNode *head;
  int size;
} Stack;

static char PASS_PREFIX[] = "\033[32mPASSED\033[0m";
static char FAIL_PREFIX[] = "\033[31mFAILED\033[0m";

static const Test NULL_TEST = {.name = NULL, .fn = NULL};

static Stack s = {0};

/*
 * Push a test to the stack
 * Returns false if op failed, else true
 * */
static bool push(Test test) {
  if (test.name == NULL) {
    fprintf(stderr, "Test name is NULL\n");
    return false;
  }

  StackNode *node = malloc(sizeof(*node));

  if (node == NULL) {
    perror("malloc");
    return false;
  }

  node->test = test;
  node->next = s.head;

  s.head = node;
  s.size++;

  return true;
}

/*
 * Pop test stack in LIFO order
 * If no tests are remaining,
 * then return NULL_TEST sentinel value
 * */
static Test pop(void) {

  if (s.head == NULL) {
    return NULL_TEST;
  }

  StackNode *node = s.head;
  StackNode *next = s.head->next;

  Test test = node->test;
  s.head = next;
  s.size--;

  free(node);

  return test;
}

/*
 * Peek into stack and return latest test
 *
 * If no test is found then return NULL_TEST
 * */
static Test peek(void) {
  if (s.head == NULL) {
    return NULL_TEST;
  }

  StackNode *node = s.head;
  Test test = node->test;

  return test;
}

bool register_test(test_fn fn, const char *name) {
  if (name == NULL) {
    fprintf(stderr, "NULL passed for test registration\n");
    return false;
  }

  if (fn == NULL) {
    fprintf(stderr, "Function pointer is NULL\n");
    return false;
  }

  Test t = {.fn = fn, .name = name};
  if (!push(t)) {
    fprintf(stderr, "Failed to push function %s %s", name, " to the stack\n");
    return false;
  }
  return true;
}

static int fork_and_run_process(Test t) {
  fflush(NULL);
  pid_t pid = fork();

  if (pid < 0) {
    perror("Fork failed");
    return -1;
  }

  if (pid == 0) {
    // child
    t.fn();
    fflush(NULL);
    _exit(0);
  }

  else {
    // parent
    int status;

    if (waitpid(pid, &status, 0) == -1) {
      perror("Waitpid failed");
      return -1;
    }

    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      fprintf(stderr, "Test %s was killed by signal %d\n", t.name,
              WTERMSIG(status));
      return -1;
    }
  }
  return -1;
}

int run_all(void) {
  int failures = 0;
  const int test_count = s.size;
  for (;;) {
    Test t = pop();
    if ((t.name == NULL) || (t.fn == NULL)) {
      break;
    }

    int result = fork_and_run_process(t);
    if (result != 0) {
      printf("%s: %s\n", FAIL_PREFIX, t.name);
      failures++;
    } else {
      printf("%s: %s\n", PASS_PREFIX, t.name);
    }
  }
  printf("%d tests run, %d failed\n", test_count, failures);
  return failures;
}

#endif
