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
#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


static void usage(void) {
  fprintf(stderr,
          "\nUsage: setuid <username[:groupname]> <program> [args...]\n");
  exit(100);
}


static int all_digits(const char *s) {
  for (; *s; s++) {
    if (!isdigit((unsigned char)*s)) {
      return 0;
    }
  }
  return 1;
}


int main(int argc, char **argv) {
  if (argc < 3) {
    usage();
  }

  char *user = argv[1];
  char *group = strchr(user, ':');
  if (!group) group = strchr(user, '.');
  if (group) *(group++) = '\0';

  uid_t uid = -1;
  gid_t gid = -1;
  int gid_ok = 0;

  if (group) {
    struct group *grent = getgrnam(group);
    if (grent) {
      gid = grent->gr_gid;
    } else if (all_digits(group)) {
      gid = (gid_t)atol(group);
    } else {
      fprintf(stderr, "%s: invalid group name (%s) specified.\n",
              argv[0], group);
      usage();
    }
    gid_ok = 1;
  }

  struct passwd *pwent = getpwnam(user);
  if (pwent) {
    uid = pwent->pw_uid;
    if (!gid_ok) {
      gid = pwent->pw_gid;
      gid_ok = 1;
    }
  } else if (all_digits(user)) {
    uid = (uid_t)atoi(user);
  } else {
    fprintf(stderr, "%s: invalid user name (%s) specified.\n",
            argv[0], user);
    usage();
  }

  if (!gid_ok) {
    fprintf(stderr,
            "%s: must specify an explicit gid when using numeric uid (%s).\n",
            argv[0], user);
    usage();
  }

  if (uid == (uid_t)-1 || gid == (gid_t)-1 || uid == 0 || gid == 0) {
    fprintf(stderr, "%s: neither uid (%ld) nor gid (%ld) may be 0 or -1.\n",
            argv[0], (long)uid, (long)gid);
    usage();
  }

  if (setgroups(0, NULL) != 0) {
    // Disable all supplementary groups.  Alternatively we could use
    // initgroups() to set all the groups associated with the given username,
    // but that could end up granting non-obvious extra privileges versus
    // what's provided on the command line.  Since this program is intended
    // for dropping privileges, let's not use any supplementary groups.
    perror("setgroups");
    return 101;
  }
  if (setgid(gid) != 0) {
    perror("setgid");
    return 102;
  }
  if (setuid(uid) != 0) {
    perror("setuid");
    return 103;
  }

  if (execvp(argv[2], argv + 2) != 0) {
    perror("execvp");
    return 104;
  }

  // NOTREACHED
}
