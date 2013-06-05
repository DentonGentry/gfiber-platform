#ifndef IOPRIO_H
#define IOPRIO_H

/* IOPRIO_* definitions copied from the standard
 * include/linux/ioprio.h header file */

#define IOPRIO_WHO_PROCESS      1
#define IOPRIO_CLASS_NONE       0
#define IOPRIO_CLASS_RT         1
#define IOPRIO_CLASS_BE         2
#define IOPRIO_CLASS_IDLE       3
#define IOPRIO_CLASS_SHIFT      (13)
#define IOPRIO_PRIO_MASK        ((1UL << IOPRIO_CLASS_SHIFT) - 1)
#define IOPRIO_PRIO_CLASS(mask) ((mask) >> IOPRIO_CLASS_SHIFT)
#define IOPRIO_PRIO_DATA(mask)  ((mask) & IOPRIO_PRIO_MASK)
#define IOPRIO_PRIO_VALUE(cls, data)  (((cls) << IOPRIO_CLASS_SHIFT) | (data))

static inline int ioprio_set(int which, int who, int ioprio) {
  return syscall(SYS_ioprio_set, which, who, ioprio);
}
static inline int ioprio_get(int which, int who) {
  return syscall(SYS_ioprio_get, which, who);
}

#endif
