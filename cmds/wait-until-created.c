/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 * Author: apenwarr@google.com (Avery Pennarun)
 *
 * A simple program that exits as soon as any of the files specified on
 * the command line exists.  (It might be nice to have it exit as soon as
 * any of the files *change*, but there's no good way to do that without
 * a race condition; the file might change while this program is starting up.)
 */
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>


static void close_inotify(int inotify) {
  // TODO(apenwarr): This fork() is silly, but helps on my workstation.
  //   For some reason, on my Ubuntu workstation, closing the inotify socket
  //   takes about 300ms for no good reason.  On a Debian kernel and another
  //   kernel I tried, there is (correctly) no delay.  This quick hack causes
  //   the fd to actually be closed in a child process instead of the parent
  //   process, so the child has to suffer the delay, but nobody cares, and
  //   the parent can exit quickly.
  if (fork() == 0) {
    close(inotify);
    _exit(0);
  }
  sleep(0);
  close(inotify);
}


static void die(const char *tag) {
  perror(tag);
  exit(1);
}


static void close_and_die(int inotify, const char *tag) {
  close_inotify(inotify);
  die(tag);
}


// TODO(apenwarr): this is also only needed because of the weird ubuntu bug.
//   Remove it in case my workstation kernel gets fixed, because normal
//   people probably don't have this problem.
static int close_on_signal = -1;
static void on_signal(int signum) {
  if (close_on_signal >= 0)
    close_inotify(close_on_signal);
  signal(signum, SIG_DFL);
  kill(getpid(), signum);
  _exit(99);
}


int main(int argc, const char **argv) {
  const int want = IN_MOVE | IN_CREATE | IN_DELETE;
  int inotify, i, err, len, *descriptors = NULL;
  char buf[4096], *ptr;
  struct inotify_event *event;

  if (argc < 2) {
    fprintf(stderr,
            "usage: %s <filenames...>\n"
            "  Waits until any of the given files has been created.\n",
            argv[0]);
    exit(2);
  }

  signal(SIGTERM, on_signal);
  signal(SIGINT, on_signal);

  while (1) {
    close_on_signal = inotify = inotify_init();
    if (inotify < 0)
      die("inotify_init");

    descriptors = calloc(argc, sizeof(int));

    for (i = 1; i < argc; ++i) {
      char *fn = strdup(argv[i]);
      const char *dir = dirname(fn);
      int error_printed = 0;
      do {
        err = inotify_add_watch(inotify, dir, want);
        if (err < 0) {
          if (errno == ENOENT) {
            if (!error_printed) {
              perror(dir);
              fprintf(stderr, "Sleeping until directory exists...\n");
              error_printed = 1;
            }
            sleep(1);
          } else {
            close_and_die(inotify, dir);
          }
        }
      } while (err < 0);
      descriptors[i] = err;
      free(fn);
    }

    for (i = 1; i < argc; ++i) {
      // Avoid race condition: it's possible a file was created *before*
      // we registered for its watch.
      if (0 == access(argv[i], F_OK))
        goto success;
    }

    while (1) {
      len = read(inotify, buf, sizeof(buf));
      if (len == 0)
        close_and_die(inotify, "inotify read EOF");
      if (len < 0) {
        if (errno == EINTR || errno == EAGAIN)
          continue;
        close_and_die(inotify, "inotify read");
      }
      for (ptr = buf; ptr < buf + len; ptr += event->len + sizeof(*event)) {
        event = (struct inotify_event *)ptr;
        for (i = 1; i < argc; ++i) {
          char *fn = strdup(argv[i]);
          const char *file = basename(fn);
          if (event->mask & (IN_IGNORED | IN_Q_OVERFLOW | IN_UNMOUNT)) {
            fprintf(stderr, "inotify: unexpected flag %X; try again.\n",
                    event->mask);
            free(fn);
            goto try_again;
          }
          if (event->wd == descriptors[i] &&
              event->len > 0 &&
              0 == strncmp(event->name, file, event->len)) {
            if ((event->mask & want) && !(event->mask & ~want)) {
              free(fn);
              goto success;
            } else {
              fprintf(stderr, "%s: expected mask 0 < 0x%X <= 0x%X; try again.\n",
                      argv[i], event->mask, want);
              free(fn);
              goto try_again;  // something unexpected happened
            }
          }
          free(fn);
        }
      }
    }
try_again:
    free(descriptors);
    close_on_signal = -1;
    close_inotify(inotify);
  }
success:
  free(descriptors);
  close_on_signal = -1;
  close_inotify(inotify);
  return 0;
}
