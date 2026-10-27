#ifndef TEST_OUTPUT_H
#define TEST_OUTPUT_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  FILE *file;
  int target;
  int saved;
} OutputCapture;

static OutputCapture capture_output(int target) {
  fflush(NULL);
  OutputCapture capture = {.file = tmpfile(), .target = target};
  assert(capture.file != NULL);
  capture.saved = dup(target);
  assert(capture.saved != -1);
  assert(dup2(fileno(capture.file), target) != -1);
  return capture;
}

static FILE *restore_output(OutputCapture capture) {
  fflush(NULL);
  assert(dup2(capture.saved, capture.target) != -1);
  close(capture.saved);
  assert(fseek(capture.file, 0, SEEK_SET) == 0);
  return capture.file;
}

static void expect_output(OutputCapture capture, const char *expected) {
  FILE *file = restore_output(capture);
  bool matches = true;
  for (size_t i = 0; expected[i] != '\0'; i++) {
    if (fgetc(file) != (unsigned char)expected[i]) {
      matches = false;
      break;
    }
  }
  if (matches && fgetc(file) != EOF)
    matches = false;
  if (!matches) {
    fprintf(stderr, "Unexpected captured output; expected:\n%sActual:\n", expected);
    rewind(file);
    int c;
    while ((c = fgetc(file)) != EOF)
      fputc(c, stderr);
  }
  fclose(file);
  assert(matches);
}

#endif
