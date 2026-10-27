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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct StackNode {
  Test test;
  struct StackNode *next;
} StackNode;

typedef struct {
  StackNode *head;
  int size;
} Stack;

typedef struct {
  int fd;
  char *output_buffer;
  size_t output_used;
  size_t output_capacity;
} PipeFd;

typedef struct {
  pid_t *groups;
  pid_t *child_id;
  const char **names;
  struct timespec *deadlines;

  PipeFd *stdout_fds;
  PipeFd *stderr_fds;
  PipeFd *stdin_fds;

  bool *exited;
  bool *timed_out;
  int *wait_status;

  size_t count;
} Children;

static char PASS_PREFIX[] = "\033[32mPASSED\033[0m";
static char FAIL_PREFIX[] = "\033[31mFAILED\033[0m";
static char YELLOW_CODE[] = "\033[1;33m";
static char RESET_TERM[] = "\033[0m";

static const Test NULL_TEST = {.name = NULL, .fn = NULL};
static const Test ERROR_TEST = {.name = "ERROR", .fn = NULL};

static Stack s = {0};
static Children running_children = {0};

/*
 * Push a test to the stack
 * The name must be heap-allocated and exclusively owned by the caller.
 * Success transfers name ownership to the stack; failure leaves it with
 * the caller. The function pointer is borrowed.
 * Returns false if op failed, else true
 * */
static bool push(Stack *stack, Test test) {
  if (stack == NULL) {
    fprintf(stderr, "Stack pointer is NULL\n");
    return false;
  }

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
  node->next = stack->head;

  stack->head = node;
  stack->size++;

  return true;
}

/*
 * Pop stack of tests in LIFO order
 * Transfers name ownership to the caller, who must free it.
 * Sentinel names are borrowed and must not be freed.
 *
 * Returns:
 * - NULL_TEST: sentinel value if no tests remaining
 * - ERROR_TEST: if the stack pointer is NULL
 * */
static Test pop(Stack *stack) {
  if (stack == NULL) {
    fprintf(stderr, "Stack pointer is NULL\n");
    return ERROR_TEST;
  }

  if (stack->head == NULL) {
    return NULL_TEST;
  }

  StackNode *node = stack->head;
  StackNode *next = stack->head->next;

  Test test = node->test;
  stack->head = next;
  stack->size--;

  free(node);

  return test;
}

/*
 * Peek into stack and return latest test
 * Borrows the name until that entry is popped or the stack is cleaned.
 * The caller must not free the borrowed name.
 *
 * If no test is found then return NULL_TEST
 * Returns ERROR_TEST if the stack pointer is NULL
 * */
static inline Test peek(const Stack *stack) {
  if (stack == NULL) {
    fprintf(stderr, "Stack pointer is NULL\n");
    return ERROR_TEST;
  }

  if (stack->head == NULL) {
    return NULL_TEST;
  }

  StackNode *node = stack->head;
  Test test = node->test;

  return test;
}

/*
 * Return no. of elements in stack
 * */
static inline int get_size(const Stack *stack) { return stack->size; }

int reap_child(pid_t pid, Test t, bool kill_child);
/*
 * Keep popping elements from stack until it's empty
 * Frees each node and its owned name.
 *
 * */
static inline bool cleanStack(Stack *stack) {
  if (stack == NULL) {
    fprintf(stderr, "Stack pointer is NULL\n");
    return false;
  }
  while (get_size(stack) > 0) {
    Test t = pop(stack);
    if (t.name == NULL_TEST.name && t.fn == NULL_TEST.fn) {
      return false;
    }
    if (t.name == ERROR_TEST.name && t.fn == ERROR_TEST.fn) {
      return false;
    }
    free((void *)t.name);
  }

  return true;
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

  const size_t BUFF_SIZE = strlen(name) + 1;
  const char *local_cpy = NULL;

  {
    char *tmp = malloc(BUFF_SIZE);

    if (tmp == NULL) {
      perror("malloc");
      return false;
    }

    memcpy(tmp, name, BUFF_SIZE);

    local_cpy = tmp;
  }

  Test t = {.fn = fn, .name = local_cpy};
  if (!push(&s, t)) {
    fprintf(stderr, "Failed to push function %s %s", name, " to the stack\n");
    free((void *)local_cpy);
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
    if (setpgid(0, 0) < 0) {
      perror("setgpid failed");
      _exit(1);
    }
    t.fn();
    fflush(NULL);
    _exit(0);
  }

  else {
    // parent
    if (setpgid(pid, pid)) {
      if (errno != EACCES && errno != ESRCH) {
        perror("Parent set gpid failed unexpectedly");

        (void)reap_child(pid, t, true);
        return -1;
      }
    }

    return reap_child(pid, t, false);
  }
  return -1;
}

int reap_child(pid_t pid, Test t, bool kill_child) {
  if (kill_child == true) {
    if (kill(pid, SIGKILL) < 0) {
      if (errno != ESRCH) {
        fprintf(stderr, "Failed to kill child\n");
        return -1;
      }
    }
  }
  int status;
  pid_t waited;
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited == -1 && errno == EINTR);

  if (waited == -1) {
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
  return 0;
}

void bulk_free_children_state(Children *temp) {
  for (size_t i = 0; i < temp->count; i++) {
    if (temp->stdout_fds != NULL)
      free(temp->stdout_fds[i].output_buffer);
    if (temp->stderr_fds != NULL)
      free(temp->stderr_fds[i].output_buffer);
    if (temp->stdin_fds != NULL)
      free(temp->stdin_fds[i].output_buffer);
  }
  free(temp->groups);
  free(temp->child_id);
  free(temp->names);
  free(temp->deadlines);
  free(temp->stdout_fds);
  free(temp->stdin_fds);
  free(temp->stderr_fds);
  free(temp->exited);
  free(temp->timed_out);
  free(temp->wait_status);
  *temp = (Children){0};
}

int initialise_children(const int test_count) {
  if (test_count <= 0) {
    fprintf(stderr, "Nothing to run, 0 test count\n");
    return -1;
  }
  Children temp = {
      .count = (size_t)test_count,
      .groups = malloc((size_t)test_count * sizeof *running_children.groups),
      .child_id =
          malloc((size_t)test_count * sizeof *running_children.child_id),
      .names = malloc((size_t)test_count * sizeof *running_children.names),
      .deadlines =
          malloc((size_t)test_count * sizeof *running_children.deadlines),
      .stdout_fds =
          malloc((size_t)test_count * sizeof *running_children.stdout_fds),
      .stderr_fds =
          malloc((size_t)test_count * sizeof *running_children.stderr_fds),
      .stdin_fds =
          malloc((size_t)test_count * sizeof *running_children.stdin_fds),
      .exited = malloc((size_t)test_count * sizeof *running_children.exited),
      .timed_out =
          malloc((size_t)test_count * sizeof *running_children.timed_out),
      .wait_status =
          malloc((size_t)test_count * sizeof *running_children.wait_status),

  };

  // Initialize every available stream array before cleanup can inspect it.
  for (size_t i = 0; i < temp.count; i++) {
    if (temp.stdout_fds != NULL)
      temp.stdout_fds[i] = (PipeFd){.fd = -1};
    if (temp.stderr_fds != NULL)
      temp.stderr_fds[i] = (PipeFd){.fd = -1};
    if (temp.stdin_fds != NULL)
      temp.stdin_fds[i] = (PipeFd){.fd = -1};
  }

  if (temp.groups == NULL || temp.child_id == NULL || temp.names == NULL ||
      temp.deadlines == NULL || temp.stdout_fds == NULL ||
      temp.stderr_fds == NULL || temp.stdin_fds == NULL ||
      temp.exited == NULL || temp.timed_out == NULL ||
      temp.wait_status == NULL) {
    fprintf(stderr, "Malloc error while trying to intiliase children state\n");
    bulk_free_children_state(&temp);
    return -1;
  }

  for (size_t i = 0; i < temp.count; i++) {
    const size_t capacity = 4096;
    temp.stdout_fds[i].output_buffer = malloc(capacity);
    temp.stderr_fds[i].output_buffer = malloc(capacity);
    if (temp.stdout_fds[i].output_buffer == NULL ||
        temp.stderr_fds[i].output_buffer == NULL) {
      fprintf(stderr, "Failed to allocate child output buffers\n");
      bulk_free_children_state(&temp);
      return -1;
    }
    temp.stdout_fds[i].output_capacity = capacity;
    temp.stderr_fds[i].output_capacity = capacity;
  }

  running_children = temp;

  return 0;
}

int run_all(void) {
  int failures = 0;
  const int test_count = get_size(&s);
  if (initialise_children(test_count) < 0) {
    fprintf(stderr, "initialise_children failed\n");
    return -1;
  }

  for (;;) {
    Test t = pop(&s);
    if ((t.name == NULL) || (t.fn == NULL)) {
      break;
    }

    int result = fork_and_run_process(t);
    if (result != 0) {
      printf("%s%s%s: %s\n", YELLOW_CODE, t.name, RESET_TERM, FAIL_PREFIX);
      failures++;
    } else {
      printf("%s%s%s: %s\n", YELLOW_CODE, t.name, RESET_TERM, PASS_PREFIX);
    }

    free((void *)t.name);
  }
  printf("%d tests run, %d failed\n", test_count, failures);
  bulk_free_children_state(&running_children);
  return failures;
}

#endif
