#ifndef __STACKTRACE_H
#define __STACKTRACE_H

/* Call this to make a stack trace from wherever you are */
void stacktrace(void);

/* Use this as your signal handler to stack trace and then exit */
void stacktrace_sighandler(int sig);

/* Call this to setup common signal handlers automatically. */
void stacktrace_setup(void);

#endif /* __STACKTRACE_H */
