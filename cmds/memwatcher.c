#define _LARGEFILE64_SOURCE
#define __STDC_FORMAT_MACROS
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef mips
#include <sys/cachectl.h>
#define CACHEFLUSH(p, l, f) cacheflush(p, l, f)
#else
#define CACHEFLUSH(p, l, f)
#endif

#define BYTES_PER_LINE  32
#define HONEYPOTPAGES   256
#define LINESIZ         64
#define PFN_BITS        55

uint8_t *honeypot = NULL;
size_t honeypotsize = 0;
long pagesize = 0;
int pagemap_fd = -1;
int kpagecount_fd = -1;
int kpageflags_fd = -1;

void initialize_memory(uint8_t *honeypot, unsigned int seed) {
  unsigned int i;
  srand(seed);
  for (i = 0; i < honeypotsize; ++i) {
    honeypot[i] = (rand() & 0xff);
  }
}

void log_page(uint8_t *mem, int len) {
  int i;

  for (i = 0; i < len; ++i) {
    printf("%02x ", mem[i]);
    if ((i % BYTES_PER_LINE) == (BYTES_PER_LINE - 1)) printf("\n");
  }
  if ((len % BYTES_PER_LINE) != (BYTES_PER_LINE - 1)) printf("\n");
}

off64_t get_proc_offset(void *addr) {
  unsigned long long vpfn = (unsigned long)addr / pagesize;
  off64_t off = vpfn * sizeof(unsigned long long);

  return off;
}

uint64_t get_pagemap(void *addr) {
  off64_t roff, off = get_proc_offset(addr);
  ssize_t rlen;
  uint64_t pagemap;

  roff = lseek64(pagemap_fd, off, SEEK_SET);
  assert(roff == off);
  rlen = read(pagemap_fd, &pagemap, sizeof(pagemap));
  assert(rlen == sizeof(pagemap));
  return pagemap;
}

uint64_t get_kpagecount(uint64_t pfn) {
  off64_t roff, off = pfn * sizeof(uint64_t);
  ssize_t rlen;
  uint64_t kpagecount;

  roff = lseek64(kpagecount_fd, off, SEEK_SET);
  assert(roff == off);
  rlen = read(kpagecount_fd, &kpagecount, sizeof(kpagecount));
  assert(rlen == sizeof(kpagecount));
  return kpagecount;
}

uint64_t get_kpageflags(uint64_t pfn) {
  off64_t roff, off = pfn * sizeof(uint64_t);
  ssize_t rlen;
  uint64_t kpageflags;

  roff = lseek64(kpageflags_fd, off, SEEK_SET);
  assert(roff == off);
  rlen = read(kpageflags_fd, &kpageflags, sizeof(kpageflags));
  assert(rlen == sizeof(kpageflags));
  return kpageflags;
}

void log_page_difference(uint8_t *honeypot, uint8_t *expected,
                         int len, unsigned int seed, int is_child) {
  uint64_t pagemap;
  uint64_t pfn;
  if (!is_child) {
    printf("Unexpected memory difference detected in parent, len=%d, "
           "seed=0x%08x\n", len, seed);
  } else {
    printf("Unexpected memory difference detected in child, len=%d, "
           "seed=0x%08x\n", len, seed);
  }
  pagemap = get_pagemap(expected);
  pfn = pagemap & ((1ULL << PFN_BITS) - 1);
  printf("Expected: %p pm=0x%" PRIx64 " kc=0x%" PRIx64 " kf=0x%" PRIx64 "\n",
         expected, pagemap, get_kpagecount(pfn), get_kpageflags(pfn));
  log_page(expected, len);

  pagemap = get_pagemap(honeypot);
  pfn = pagemap & ((1ULL << PFN_BITS) - 1);
  printf("Actual:   %p pm=0x%" PRIx64 " kc=0x%" PRIx64 " kf=0x%" PRIx64 "\n",
         honeypot, pagemap, get_kpagecount(pfn), get_kpageflags(pfn));
  log_page(honeypot, len);
  fflush(stdout);
}

void check_memory(uint8_t *honeypot, unsigned int seed, int is_child) {
  uint8_t *expected = malloc(honeypotsize);
  unsigned int i;
  long j;
  initialize_memory(expected, seed);
  for (i = 0; i < honeypotsize; i += pagesize) {
    int start = -1, end = -1;
    for (j = 0; j < pagesize; ++j) {
      if (honeypot[i+j] != expected[i+j]) {
        if (start < 0) start = i+j;
        end = i+j;
      }
    }
    if (start != -1) {
      int len = end - start + 1;
      log_page_difference(honeypot + start, expected + start,
                          len, seed, is_child);
      // flush cache and log it again.
      CACHEFLUSH(honeypot + start, len, DCACHE);
      CACHEFLUSH(expected + start, len, DCACHE);
      log_page_difference(honeypot + start, expected + start,
                          len, seed, is_child);
      // And finally regenerate the expected and log it again.
      initialize_memory(expected, seed);
      log_page_difference(honeypot + start, expected + start,
                          len, seed, is_child);
    }
  }
  free(expected);
}

void corrupt_memory(uint8_t *honeypot) {
  if ((rand() % 8) == 0) {
    int offset, len, i;
    offset = rand() % honeypotsize;
    len = rand() % 128;
    printf("Test mode corrupting bytes off=%d, len=%d\n", offset, len);
    for (i = 0; i < len; ++i) {
      honeypot[offset + i] ^= rand();
    }
  }
}

void usage(char *progname) {
  printf("usage: %s [-t] [-m #pages] [-s sleeptime]\n", progname);
  printf("\t-t\ttest mode, deliberately introduce random corruption.\n");
  printf("\t-m\tmemory to monitor, in megabytes\n");
  printf("\t-s\tnumber of seconds to sleep before checking for corruption\n");
  exit(1);
}

int main(int argc, char **argv)
{
  size_t honeypotpages = HONEYPOTPAGES;
  int testmode = 0;
  int sleeptime = -1;
  int rc, c;

  pagesize = sysconf(_SC_PAGESIZE);
  assert(pagesize > 0);
  pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  assert(pagemap_fd >= 0);
  kpagecount_fd = open("/proc/kpagecount", O_RDONLY);
  assert(kpagecount_fd >= 0);
  kpageflags_fd = open("/proc/kpageflags", O_RDONLY);
  assert(kpageflags_fd >= 0);

  while ((c = getopt(argc, argv, "tm:s:")) != -1) {
    switch(c) {
      case 't': testmode = 1; break;
      case 'm': {
        ssize_t mbytes = atoi(optarg) * 1024 * 1024;
        ssize_t pages = mbytes / pagesize;
        honeypotpages = (pages > 0) ? pages : 1;  // -m 0 == minimum memory
        break;
      }
      case 's': sleeptime = atoi(optarg); break;
      default: usage(argv[0]); break;
    }
  }

  if (sleeptime < 0) {
    sleeptime = testmode ? 2 : 600;
  }

  honeypotsize = honeypotpages * pagesize;
  printf ("Monitoring %zu bytes every %d seconds\n", honeypotsize, sleeptime);
  rc = posix_memalign((void **)&honeypot, pagesize, honeypotsize);
  assert(rc == 0);

  // Initialize to 0 to force on demand paging for the honeypot.
  // If the honeypot is not mapped into memory, then no copy on write
  // will happen for the first fork.
  memset(honeypot, 0, honeypotsize);

  while (1) {
    // Reinitialize on each loop. We only want to log corruption once.
    unsigned int seed;

    pid_t child_pid = fork();
    int is_child = child_pid == 0;
    if (child_pid == -1) {
      perror("Error forking");
    } else if (child_pid == 0) {
      close(pagemap_fd);
      // close the pagemap fd inherited from the parent
      pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
      assert(pagemap_fd >= 0);
    }

    seed = time(NULL) + child_pid;
    initialize_memory(honeypot, seed);
    CACHEFLUSH(honeypot, honeypotsize, DCACHE);
    check_memory(honeypot, seed, is_child);

    sleep(sleeptime);
    if (testmode)
      corrupt_memory(honeypot);
    check_memory(honeypot, seed, is_child);
    if (child_pid == 0) {
      exit(0);
    }

    wait(NULL);
  }
}
