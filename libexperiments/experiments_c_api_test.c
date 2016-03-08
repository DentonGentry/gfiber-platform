#include "experiments_c_api_test.h"

int test_experiments_initialize(const char *config_dir) {
  return experiments_initialize(config_dir, 0, DummyExperimentsRegisterFunc);
}

int test_experiments_is_initialized() {
  return experiments_is_initialized();
}

int test_experiments_register(const char *name) {
  return experiments_register(name);
}

int test_experiments_is_registered(const char *name) {
  return experiments_is_registered(name);
}

int test_experiments_is_enabled(const char *name) {
  return experiments_is_enabled(name);
}
