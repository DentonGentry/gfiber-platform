/*
 * This is a scriptable tool to use mmap and write to registers.
 */

#define _POSIX_SOURCE   /* for fileno! */
#define _BSD_SOURCE     /* for usleep! */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <fcntl.h>

#define MAXSLOTS  10
#define MAXARGS    10

struct slot {
  char          path[PATH_MAX];
  int           fd;
  off_t         fileLength;
  void*         map;
  uint64_t      addr;
  uint64_t      length;
};

static struct slot slots[MAXSLOTS];
static char* posPrefix = "";

void usage(const char* prog)
{
  fprintf(stderr, "Usage: %s [-q)uiet] [command-file ...]\n", prog);
  fprintf(stderr, "\twhere command-file or stdin contains:\n");
  fprintf(stderr, "\t\tfile 0 /sys/bus/pci/devices/0000:01:00.0/resource0 "
                    "0 0x10000\n");
  fprintf(stderr, "\t\tfile 2 /sys/bus/pci/devices/0000:01:00.0/resource2 "
                    "0 0x10000\n");
  fprintf(stderr, "\t\tfile 4 /sys/bus/pci/devices/0000:01:00.0/resource4 "
                    "0 0x10000\n");
  fprintf(stderr, "\t\tread 2 16 4  "
                    "# read file 2, addr 16, length 4\n");
  fprintf(stderr, "\t\twrite 4 18 4 0xffff  "
                    "# write file 4, addr 18, length 4, value 0xffff\n");
  fprintf(stderr, "\t\tdump 4 18 4 100  "
                    "# dump file 4, addr 18, length 4, 100 values\n");
  fprintf(stderr, "\t\tclose 0  "
                    "# close a file\n");
}

int asUnsigned(uint64_t* ip, const char* str)
{
  char* end;

  uint64_t value = strtoull(str, &end, 0);
  if (end == str || *end != '\0') {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "failed to parse '%s' as unsigned (eg 0x10 or 16)\n", str);
    return -1;
  }
  *ip = value;
  return 0;
}

int asFileAddr(uint64_t* ip, const char* str, int slot)
{
  if (asUnsigned(ip, str) < 0) {
    return -1;
  }
  if (*ip >= (uint64_t) slots[slot].fileLength) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "address '%s' exceeds bounds of file\n", str);
    return -1;
  }
  return 0;
}

int asFileLength(uint64_t* ip, const char* str, int slot, uint64_t fileAddr)
{
  if (asUnsigned(ip, str) < 0) {
    return -1;
  }
  if (fileAddr + *ip >= (uint64_t) slots[slot].fileLength) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "length '%s' exceeds bounds of file\n", str);
    return -1;
  }
  return 0;
}

int asAddr(uint64_t* ip, const char* str, int slot)
{
  if (asUnsigned(ip, str) < 0) {
    return -1;
  }
  if (*ip >= (uint64_t) slots[slot].length) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "address '%s' out of range 0x%" PRIx64 "..0x%" PRIx64 "\n",
            str, slots[slot].addr, slots[slot].addr + slots[slot].length-1);
    return -1;
  }
  return 0;
}

int asLength(uint64_t* ip, const char* str, int slot, uint64_t addr)
{
  if (asUnsigned(ip, str) < 0) {
    return -1;
  }
  if (addr + *ip >= addr + slots[slot].length) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "address '%s' out of range 0..0x%" PRIx64 "\n",
            str, slots[slot].length-1);
    return -1;
  }
  return 0;
}

int asWordLen(int *ip, const char* str)
{
  uint64_t value;
  if (asUnsigned(&value, str) < 0) {
    return -1;
  }
  if (value != 1 && value != 2 && value != 4 && value != 8) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "length '%s' must be 1, 2, 4 or 8\n", str);
    return -1;
  }
  *ip = value;
  return 0;
}

int asSlot(int *ip, const char* str, int wantOpen)
{
  uint64_t value;
  if (asUnsigned(&value, str) < 0) {
    return -1;
  }
  if (value >= MAXSLOTS) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "slot '%s' is out of range 0-%d or is not open\n",
            str, MAXSLOTS-1);
    return -1;
  }
  if (wantOpen && slots[value].map == NULL) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "slot '%s' is not open\n", str);
    return -1;
  } else if (!wantOpen && slots[value].map != NULL) {
    fprintf(stderr, "%s", posPrefix);
    fprintf(stderr, "slot '%s' is already open\n", str);
    return -1;
  }
  *ip = value;
  return 0;
}

int do_open(char* path, int slot, uint64_t fileAddr, uint64_t length)
{
  int fd = open(path, O_RDWR);
  if (fd < 0) {
    perror(path);
    return -1;
  }

  struct stat stats;
  if (fstat(fd, &stats) < 0) {
    perror("fstat");
    close(fd);
    return -1;
  }
  uint64_t fileLength = stats.st_size;

  if (fileAddr + length > fileLength) {
    fprintf(stderr, "mapped range (0x%" PRIx64 ",0x%" PRIx64 ") "
                      "is outside of size of file (0x%" PRIx64 ")\n",
            fileAddr, length, fileLength);
    close(fd);
    return -1;
  }

  void* mm = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, fileAddr);
  if (mm == NULL) {
    perror("mmap");
    close(fd);
    return -1;
  }
  strncpy(slots[slot].path, path, sizeof slots[slot].path);
  slots[slot].fd = fd;
  slots[slot].fileLength = fileLength;
  slots[slot].map = mm;
  slots[slot].addr = fileAddr;
  slots[slot].length = length;
  return 0;
}

void do_close(int slot) {
  munmap(slots[slot].map, slots[slot].length);
  close(slots[slot].fd);
  memset(&slots[slot], 0, sizeof (slots[slot]));
  slots[slot].fd = -1;
}

int do_read_helper(int slot, uint64_t addr, int wordlen, uint64_t* valueP)
{
  void* vp = slots[slot].map + addr;
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  void *dp;

  switch (wordlen) {
  case sizeof(u64):     dp = &u64; break;
  case sizeof(u32):     dp = &u32; break;
  case sizeof(u16):     dp = &u16; break;
  case sizeof(u8):      dp = &u8; break;
  default:
        fprintf(stderr, "Can't find datatype for wordlen '%d'\n", wordlen);
        return -1;
  }

  memcpy(dp, vp, wordlen);

  switch (wordlen) {
  case sizeof(u64):     *valueP = u64; break;
  case sizeof(u32):     *valueP = u32; break;
  case sizeof(u16):     *valueP = u16; break;
  case sizeof(u8):      *valueP = u8; break;
  default:
        fprintf(stderr, "Can't find datatype for wordlen '%d'\n", wordlen);
        return -1;
  }
  return 0;
}

int do_read(int slot, uint64_t addr, int wordlen)
{
  uint64_t value;

  if (do_read_helper(slot, addr, wordlen, &value) < 0) {
    return -1;
  }
  printf("0x%0*" PRIx64 "\n", 2*wordlen, value);
  return 0;
}

void do_write(int slot, uint64_t addr, int wordlen, uint64_t value)
{
  void* vp = slots[slot].map + addr;
  memcpy(vp, &value, wordlen);
}

int do_hexdump(int slot, uint64_t addr, int wordlen, uint64_t count)
{
  uint64_t perline = 16/wordlen;
  int err = 0;

  for (uint64_t i = 0; i < count; i += perline) {
    printf("%08" PRIx64 ":", addr + i * wordlen);
    for (uint64_t j = 0; j < perline; j++) {
      if (i + j >= count) {
        break;
      }
      uint64_t value = 0;
      if (do_read_helper(slot, addr + (i + j) * wordlen, wordlen, &value) < 0) {
        err = -1;
      }
      printf(" 0x%0*" PRIx64, 2*wordlen, value);
    }
    printf("\n");
  }
  return err;
}

// loop waiting for a register to have a value
int wait_for_bits(uint32_t* creg, uint32_t mask, uint32_t want,
                  int usec_delay, int max_tries)
{
  uint32_t got = 0;

  for (int i = 0; i < max_tries; i++) {
    got = *creg;
    if ((got & mask) == want) {
      return 0;
    }
    usleep(usec_delay);
  }
  fprintf(stderr, "%s", posPrefix);
  fprintf(stderr, "timeout waiting for bits: "
                  "mask=0x%08x want=0x%08x got=0x%08x\n", mask, want, got);
  return -1;
}

// Alleycat XSMI Management Register

#define CMD_ADDR_THEN_WRITE             5
#define CMD_ADDR_THEN_READ              7

#define CMD_BUSY                        (1 << 30)

// read from device on MDIO bus attached to a PCI device
int do_mread(int slot, uint64_t addr_reg, uint64_t cmd_reg,
             uint64_t port, uint64_t dev, uint64_t reg, uint64_t count)
{
  uint32_t* areg = slots[slot].map + addr_reg;
  uint32_t* creg = slots[slot].map + cmd_reg;
  uint32_t value;

  for (uint64_t i = 0; i < count; i++) {
    uint32_t rreg = reg + i;

    if (wait_for_bits(creg, CMD_BUSY, 0, 1000, 100) < 0) {
      return -1;
    }

    // remote register we want to read
    *areg = rreg;

    // issue command
    *creg = (CMD_ADDR_THEN_READ << 26) | (dev << 21) | (port << 16);

    if (wait_for_bits(creg, CMD_BUSY, 0, 1000, 100) < 0) {
      return -1;
    }

    // get result
    value = *creg;

    printf("%" PRIx64 ".%" PRIx64 ".%04" PRIx32 ": %04x %04x\n",
           port, dev, rreg, value >> 16, value & 0xffff);
  }
  return 0;
}

int cmd_open(int ac, char* av[])
{
  char* usage = "open slot file offset length";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }
  if (ac != 5) {
    fprintf(stderr, "Usage: %s\n", usage);
    return -1;
  }

  int slot;
  char* path = av[2];
  uint64_t offset;
  uint64_t len;

  if (asSlot(&slot, av[1], 0) < 0 ||
      asUnsigned(&offset, av[3]) < 0 ||
      asUnsigned(&len, av[4]) < 0) {
    return -1;
  }
  if (do_open(path, slot, offset, len) < 0) {
    return -1;
  }
  return 0;
}

int cmd_close(int ac, char* av[])
{
  char* usage = "close slot";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }
  if (ac != 2) {
    fprintf(stderr, "Usage: %s\n", usage);
    return -1;
  }

  int slot;

  if (asSlot(&slot, av[1], 1) < 0) {
    return -1;
  }
  do_close(slot);
  return 0;
}

int cmd_read(int ac, char* av[])
{
  char* usage = "read slot addr wordlen";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }
  if (ac != 4) {
    fprintf(stderr, "Usage: %s\n", usage);
    return -1;
  }

  int slot;
  uint64_t addr;
  int wordlen;

  if (asSlot(&slot, av[1], 1) < 0 ||
      asAddr(&addr, av[2], slot) < 0 ||
      asWordLen(&wordlen, av[3]) < 0) {
    return -1;
  }
  if (do_read(slot, addr, wordlen) < 0) {
    return -1;
  }
  return 0;
}

int cmd_write(int ac, char* av[])
{
  char* usage = "write slot addr wordlen value";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }
  if (ac != 5) {
    fprintf(stderr, "Usage: %s\n", usage);
    return -1;
  }

  int slot;
  uint64_t addr;
  int wordlen;
  uint64_t value;

  if (asSlot(&slot, av[1], 1) < 0 ||
      asAddr(&addr, av[2], slot) < 0 ||
      asWordLen(&wordlen, av[3]) < 0 ||
      asUnsigned(&value, av[4]) < 0) {
    return -1;
  }
  do_write(slot, addr, wordlen, value);
  return 0;
}

int cmd_dump(int ac, char* av[])
{
  char* usage = "dump slot addr wordlen count";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }
  if (ac != 5) {
    fprintf(stderr, "Usage: %s\n", usage);
    return -1;
  }

  int slot;
  uint64_t addr;
  int wordlen;
  uint64_t count;

  if (asSlot(&slot, av[1], 1) < 0 ||
      asUnsigned(&addr, av[2]) < 0 ||
      asWordLen(&wordlen, av[3]) < 0 ||
      asUnsigned(&count, av[4]) < 0) {
    return -1;
  }
  if (do_hexdump(slot, addr, wordlen, count) < 0) {
    return -1;
  }
  return 0;
}

int cmd_msleep(int ac, char* av[])
{
  char* usage = "msleep msecs";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }
  if (ac != 2) {
    fprintf(stderr, "Usage: %s\n", usage);
    return -1;
  }

  uint64_t msecs;

  if (asUnsigned(&msecs, av[1]) < 0) {
    return -1;
  }
  usleep(msecs*1000);
  return 0;
}

int cmd_echo(int ac, char* av[])
{
  char* usage = "echo text ...";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }

  for (int i = 1; i < ac; i++) {
    printf("%s ", av[i]);
  }
  printf("\n");
  return 0;
}

// read registers on mdio devices via PCI indirection
int cmd_mread(int ac, char* av[])
{
  char* usage = "mread slot addr_reg cmd_reg port dev reg count";

  if (ac > 1 && strcmp(av[1], "help") == 0) {
    printf("\t%s\n", usage);
    return 0;
  }
  if (ac != 8) {
    fprintf(stderr, "Usage: %s\n", usage);
    return -1;
  }

  int slot;
  uint64_t addr_reg;
  uint64_t cmd_reg;
  uint64_t port;
  uint64_t dev;
  uint64_t reg;
  uint64_t count;

  if (asSlot(&slot, av[1], 1) < 0 ||
      asAddr(&addr_reg, av[2], slot) < 0 ||
      asAddr(&cmd_reg, av[3], slot) < 0 ||
      asUnsigned(&port, av[4]) < 0 ||
      asUnsigned(&dev, av[5]) < 0 ||
      asUnsigned(&reg, av[6]) < 0 ||
      asUnsigned(&count, av[7]) < 0) {
    return -1;
  }
  if (do_mread(slot, addr_reg, cmd_reg, port, dev, reg, count) < 0) {
    return -1;
  }
  return 0;
}

typedef int (*cmdFunc)(int ac, char* av[]);

struct commands {
  char*  name;
  cmdFunc  func;
};

struct commands cmds[] = {
  { "open", cmd_open, },
  { "close", cmd_close, },
  { "read", cmd_read, },
  { "write", cmd_write, },
  { "dump", cmd_dump, },
  { "msleep", cmd_msleep, },
  { "echo", cmd_echo, },
  { "mread", cmd_mread, },
  { NULL, NULL },
};

int processFile(const char* file, int quiet)
{
  const char* fpName;
  FILE* fp;
  char* delim= " \t\n";

  if (file == NULL || strcmp(file, "-") == 0) {
    fpName = "stdin";
    fp = stdin;
  } else {
    fpName = file;
    fp = fopen(fpName, "r");
    if (fp == NULL) {
      perror(fpName);
      return -1;
    }
  }

  int isTTY = isatty(fileno(fp));

  int lineno = 0;
  int err = 0;
  for (;;) {
    char line[1024] = { 0 };
    if (isTTY) {
      printf("mmap>> ");
    }
    if (fgets(line, sizeof line, fp) == NULL) {
      break;    // EOF
    }

    lineno++;
    char fileLine[128];
    sprintf(fileLine, "%s:%d: ", fpName, lineno);
    posPrefix = fileLine;

    int len = strlen(line);
    if (len == 0) {
      continue;
    }
    if (line[len-1] != '\n') {
      fprintf(stderr, "%s", posPrefix);
      fprintf(stderr, "line too long\n");
      err++;
      continue;
    }
    if (line[0] == '#') {
      continue;    // skip comments
    }

    if (!quiet) {
      printf("# %s", line);
    }

    int ac = 0;
    char* av[MAXARGS];
    char** avP = av;

    *avP = strtok(line, delim);
    while (*avP != NULL) {
      if (ac >= MAXARGS) {
        break;
      }
      ac++;
      avP++;
      *avP = strtok(NULL, delim);
    }
    if (ac > MAXARGS) {
      fprintf(stderr, "%s", posPrefix);
      fprintf(stderr, "too many arguments\n");
      err++;
      continue;
    }
    if (ac == 0) {
      continue;  // skip whitespace only
    }

    if (strcmp(av[0], "help") == 0) {
      char* args[] = { "", "help" };
      for (struct commands* cp = cmds; cp->func != NULL; cp++) {
        cp->func(2, args);
      }
      continue;
    }

    int match = 0;
    for (struct commands* cp = cmds; cp->func != NULL; cp++) {
      if (strcmp(av[0], cp->name) == 0) {
        if (cp->func(ac, av) < 0) {
          fprintf(stderr, "%s", posPrefix);
          fprintf(stderr, "command '%s' failed\n", av[0]);
          err++;
        }
        match = 1;
        break;
      }
    }
    if (!match) {
      fprintf(stderr, "%s", posPrefix);
      fprintf(stderr, "unknown command '%s', try help\n", av[0]);
      err++;
      continue;
    }
  }
  return err ? -1 : 0;
}

int main(int argc, char* argv[])
{
  int err = 0;
  int quiet = 0;

  int opt;
  while ((opt = getopt(argc, argv, "q")) != -1) {
    switch (opt) {
    case 'q':
      quiet = 1;
      break;
    default:
      usage(argv[0]);
      exit(1);
    }
  }

  setbuf(stdout, NULL);         // unbuffered

  if (optind == argc) {
    if (processFile(NULL, quiet) < 0) {
      err++;
    }
  } else {
    for (int i = optind; i < argc; i++) {
      if (processFile(argv[i], quiet) < 0) {
        err++;
      }
    }
  }
  exit(err ? 1 : 0);
}
