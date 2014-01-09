#include <string.h>

#include "utils.h"


// Strip underscores from the sring.
void strip_underscores(char *line) {
  if (!line) return;

  if (strchr(line, '_')) {
    char *src=line;
    char *dst=line;
    while (*src) {
      *dst = *src++;
      if (*dst != '_')
        ++dst;
    }
    *dst = 0;
  }
}
