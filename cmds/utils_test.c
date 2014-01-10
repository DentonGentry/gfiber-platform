#include <stdio.h>
#include <string.h>

#include "utils.h"

#define ARRAYSIZE(x) (int)(sizeof(x)/sizeof(x[0]))

int main() {
  int i;
  char *tests[] = {
    "a", "abc", "_a", "abc_", "_", "____",
  };
  char *expect[] = {
    "a", "abc", "a", "abc", "", "",
  };
  int fail = 0;

  for (i = 0; i < ARRAYSIZE(tests); ++i) {
    tests[i] = strdup(tests[i]);
    strip_underscores(tests[i]);
    if (strcmp(tests[i], expect[i])) {
      printf("Failed: expected: %s got: %s\n", tests[i], expect[i]);
      fail = 1;
    }
    printf("stripped version: '%s'\n", tests[i]);
  }

  return fail;  // an optimist would return success.
}
