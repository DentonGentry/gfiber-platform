// GFHD254 has a bug where software reset doesn't reset the entire
// chip, some state in the SAGE engine isn't getting reset.  This
// drives a gpio that connects back to the chips own external reset
// pin, resetting the chip with this pin works around the issue as
// the SAGE engine is completely reset in this path.

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define REG_BASE 0xf0410000
#define REG_SIZE 0x8000


#define GPIO_DATA (0x7404 / 4)
#define GPIO_IODIR (0x7408 / 4)
#define CTRL_MUX_0 (0x0700 / 4)
#define CTRL_MUX_1 (0x0704 / 4)

static void *mmap_(
    void* addr, size_t size, int prot, int flags, int fd,
    off_t offset) {
#ifdef __ANDROID__
  return mmap64(addr, size, prot, flags, fd,
                (off64_t)(uint64_t)(uint32_t)offset);
#else
  return mmap(addr, size, prot, flags, fd, offset);
#endif
}

// TODO(jnewlin):  Revist this after the exact gpio being used
// is settled on.

int main() {
  int fd = open("/dev/mem", O_RDWR);
  volatile uint32_t* reg;

  if (fd < 0) {
    perror("mmap");
    return 1;
  }

  reg = mmap_(NULL, REG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
              fd, REG_BASE);
  if (reg == MAP_FAILED) {
    perror("mmap");
    return 1;
  }

  // Set the pin mux to gpio, value of zero selects gpio mode, this
  // is the reset value so this is probably not required, but just
  // in case.
  reg[CTRL_MUX_0] &= ~((0xf << 8) | (0xf << 12)); // aon_gio2 and 3
  reg[CTRL_MUX_1] &= ~(0xf << 4); // aon_gio9


  // Set the direction to be an output and drive it low.
  reg[GPIO_IODIR] &= ~((1 << 2) | (1 << 3) | (1 << 9));
  reg[GPIO_DATA] &= ~((1 << 2) | (1 << 3) | (1 << 9));

  return 0;
}
