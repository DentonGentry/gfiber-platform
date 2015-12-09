// Daemon that periodically polls for a Quantenna device's /proc/kmsg data and
// prints it to stdout.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  while (true) {
    FILE *in = popen("qcsapi_pcie_static get_custom_value kmsgcat", "r");
    if (in == NULL) {
      perror("could not retrieve /proc/kmsg from Quantenna device");
      return EXIT_FAILURE;
    }

    char buf[128];
    int total = 0;
    int n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
      fwrite(buf, 1, n, stdout);
      total += n;
    }

    pclose(in);

    if (total == 0) {
      sleep(5);
    }
  }
}
