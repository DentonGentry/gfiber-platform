#ifndef __STACKTRACE_H
#define __STACKTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Call this to make a stack trace from wherever you are */
void stacktrace(void);

/* Use this as your signal handler to stack trace and then exit */
void stacktrace_sighandler(int sig);

/* Call this to setup common signal handlers automatically. */
void stacktrace_setup(void);

/* Need to export this in order to allow testing it */
char *format_uint(unsigned int i);

#ifdef __cplusplus
}
#endif

#endif /* __STACKTRACE_H */
