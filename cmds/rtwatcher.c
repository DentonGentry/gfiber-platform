#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


time_t last_tick;


static void _log(char *msg) {
  if (write(1, msg, strlen(msg)) < 0) {
    perror("write");
  }
}


static time_t monotime(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    return time(NULL);
  } else {
    return ts.tv_sec;
  }
}


static void *realtime_thread(void *arg) {
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = 99;
  if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
    perror("sched_setscheduler");
    exit(5);
  }

  time_t now = monotime(), last_printed = 0;
  int warned = 0;

  last_tick = last_printed = now;

  // high-priority thread
  while (1) {
    now = monotime();
    if (now - last_tick > 10) {
      if (!warned) {
        _log("<0>rtwatcher: WARNING: no non-realtime ticks for 10 seconds!\n");
        _log("<0>rtwatcher: process listing follows.\n");
        warned = 1;

        // print a list of all processes (multithreaded processes get one
        // line per thread) that are runnable (R or D state).  The watchdog
        // timer will probably be kicking in soon, but if we get this into
        // the log, it'll be available for analysis on the next boot.
        if (system("ps axrH -o pid,rtprio,bsdtime,state,cmd --cols=80") < 0) {
          perror("ps");
        }

        sleep(5);
        _log("<4>(5 seconds later...)\n");
        if (system("ps axrH -o pid,rtprio,bsdtime,state,cmd --cols=80") < 0) {
          perror("ps");
        }
      }
    } else if (now - last_printed > 60) {
      _log("<7>rtwatcher: ok\n");
      last_printed = now;
    } else if (warned) {
      _log("<0>rtwatcher: ...and we're back.\n");
      warned = 0;
    }
    sleep(1);
  }
  return NULL;
}


int main(int argc, char **argv)
{
  pthread_t ptid;
  errno = pthread_create(&ptid, NULL, realtime_thread, NULL);
  if (errno) {
    perror("pthread_create");
    return 1;
  }

  // low-priority thread
  while (1) {
    last_tick = monotime();
    sleep(1);
  }

  return 0;
}
