#ifndef _LIBEXPERIMENTS_EXPERIMENTS_C_API_TEST_H
#define _LIBEXPERIMENTS_EXPERIMENTS_C_API_TEST_H

// Provides C-compiled functions to test the C-API functionality. The main
// purpose of this is to verify that one can use libexperiments from a purely C
// environment.

#include "experiments.h"

#ifdef __cplusplus
extern "C" {
#endif

int test_experiments_initialize(const char *config_dir);
int test_experiments_is_initialized();
int test_experiments_register(const char *name);
int test_experiments_is_registered(const char *name);
int test_experiments_is_enabled(const char *name);

#ifdef __cplusplus
}
#endif

#endif  // _LIBEXPERIMENTS_EXPERIMENTS_C_API_TEST_H
