/*
 * Copyright 2012-2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * A simple program that uses the inotify api to watch a directory given as
 * argument. If any of the files in that directory suffers any change,
 * this program will output the name of the file, without including the
 * whole path.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

static void die(const char *tag) {
  perror(tag);
  exit(1);
}

static void close_and_die(int fd, const char *tag) {
  close(fd);
  die(tag);
}


int main(int argc, const char **argv) {
  int inotify_fd, dir_wd, len;
  char buf[4096], *ptr;
  struct inotify_event *event;

  if (argc != 2) {
    fprintf(stderr,
            "usage: %s <dirname>\n"
            " Outputs the name of the files the given "
            " directory that have been modified.\n",
            argv[0]);
    exit(2);
  }

  const char *dir_name = argv[1];

  // Sanity checks.
  struct stat sb;
  if (stat(dir_name, &sb) == 0) {
    if (!S_ISDIR(sb.st_mode)) {
      fprintf(stderr, "%s is not a directory\n", dir_name);
      exit(1);
    }
  } else {
    if (mkdir(dir_name, S_IRWXU | S_IRWXG | S_IRWXO))
      die("mkdir");
  }

  inotify_fd = inotify_init();
  if (inotify_fd < 0)
    die("inotify_init");

  dir_wd = inotify_add_watch(inotify_fd, dir_name,
                             IN_MOVE | IN_CREATE | IN_DELETE | IN_MODIFY);
  if (dir_wd < 0)
    die("inotify_add_watch");

  while (1) {
    len = read(inotify_fd, buf, sizeof(buf));
    if (len == 0) {
      fprintf(stderr, "inotify read EOF");
      break;
    }
    if (len < 0) {
      if (errno == EINTR || errno == EAGAIN)
        continue;
      close_and_die(inotify_fd, "inotify read");
    }

    for (ptr = buf; ptr < buf + len; ptr += event->len + sizeof(*event)) {

      event = (struct inotify_event *)ptr;
      // Check to see if the the event struct is not incomplete.
      if (ptr + sizeof(*event) > buf + len) {
        fprintf(stderr, "inotify: incomplete inotify event\n");
        break;
      }
      if (event->mask & (IN_IGNORED | IN_UNMOUNT)) {
        die("bailing out, non-existing directory");
      } else if (event->mask & IN_Q_OVERFLOW) {
        fprintf(stderr, "inotify: event queue overflowed\n");
        break;
      } else if (event->mask & IN_ISDIR) {
        fprintf(stderr, "inotify: directory triggered event, will ignore\n");
        continue;
      }

      if (event->len && ptr + sizeof(*event) + event->len <= buf + len) {
        // Pathname is null terminated.
        fprintf(stdout, "%s\n", event->name);
        fflush(stdout);
      }
    }
  }

  close(inotify_fd);
  return 0;
}
