#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __cplusplus
#define __STDC_FORMAT_MACROS
#endif

#include "experiments.h"

#include <inttypes.h>

#include <sstream>
#include <string>

#include "utils.h"

using namespace libexperiments_utils;  // NOLINT

Experiments *experiments = NULL;

int DefaultExperimentsRegisterFunc(const char *name) {
  std::vector<std::string> cmd({"register_experiment", name});
  std::ostringstream out, err;
  int64_t timeout_usec = secs_to_usecs(5);
  int status;
  int ret = run_cmd(cmd, "", &status, &out, &err, timeout_usec);
  if (ret < 0 || status != 0) {
    log("experiments:Error-Cannot register '%s', ret:%d status:%d stdout:%s "
        "stderr:%s", name, ret, status, out.str().c_str(),
        err.str().c_str());
    return 0;  // boolean false
  }
  return 1;  // boolean true
}

int DummyExperimentsRegisterFunc(const char *name) {
  return 1;  // boolean true
}

bool Experiments::Initialize(
    const std::string &config_dir, int64_t min_time_between_refresh_usec,
    experiments_register_func_t register_func,
    const std::vector<std::string> &names_to_register) {
  log("experiments:initializing - config_dir:%s min_time_between_refresh:%"
      PRId64 " us", config_dir.c_str(), min_time_between_refresh_usec);

  std::lock_guard<std::mutex> lock_guard(lock_);

  if (register_func == NULL) {
    log("experiments:Error-register_func is NULL");
    return false;
  }

  if (!directory_exists(config_dir.c_str())) {
    log("experiments:Error-config_dir '%s' does not exist", config_dir.c_str());
    return false;
  }

  if (min_time_between_refresh_usec < 0)
    min_time_between_refresh_usec = 0;

  config_dir_ = config_dir;
  register_func_ = register_func;
  min_time_between_refresh_usec_ = min_time_between_refresh_usec;

  initialized_ = true;  // initialization part succeeded at this point

  // register any provided experiments
  if (!names_to_register.empty()) {
    if (!Register_Locked(names_to_register))
      return false;

    // initial read of registered experiments states
    Refresh();
  }

  return true;
}

bool Experiments::Register(const std::vector<std::string> &names) {
  if (!IsInitialized()) {
    log("experiments:Cannot register, not initialized!");
    return false;
  }
  return Register_Unlocked(names);
}

bool Experiments::Register_Unlocked(const std::vector<std::string> &names) {
  std::lock_guard<std::mutex> lock_guard(lock_);
  return Register_Locked(names);
}

bool Experiments::Register_Locked(const std::vector<std::string> &names) {
  for (const auto &name : names) {
    if (IsInRegisteredList(name)) {
      log("experiments:'%s' already registered", name.c_str());
      continue;
    }

    // call external register function
    if (!register_func_(name.c_str()))
      return false;  // no reason to continue

    registered_experiments_.insert(name);
    log("experiments:Registered '%s'", name.c_str());
  }
  return true;
}

bool Experiments::IsRegistered(const std::string &name) {
  std::lock_guard<std::mutex> lock_guard(lock_);
  return IsInRegisteredList(name);
}

bool Experiments::IsEnabled(const std::string &name) {
  if (!IsInitialized())
    return false;  // silent return to avoid log flooding

  std::lock_guard<std::mutex> lock_guard(lock_);

  if (us_elapse(last_time_refreshed_usec_) >= min_time_between_refresh_usec_) {
    Refresh();
  }

  return IsInEnabledList(name);
}

void Experiments::Refresh() {
  for (const auto &name : registered_experiments_)
    UpdateState(name);
  last_time_refreshed_usec_ = us_elapse(0);
}

void Experiments::UpdateState(const std::string &name) {
  if (!IsInRegisteredList(name)) {
    log("experiments:'%s' not registered", name.c_str());
    return;
  }

  std::string file_path = config_dir_ + "/" + name + ".active";
  bool was_enabled = IsInEnabledList(name);
  bool is_enabled = file_exists(file_path.c_str());
  if (is_enabled && !was_enabled) {
    log("experiments:'%s' is now enabled", name.c_str());
    enabled_experiments_.insert(name);
  } else if (!is_enabled && was_enabled) {
    log("experiments:'%s' is now disabled", name.c_str());
    enabled_experiments_.erase(name);
  }
}


// API for C programs
int experiments_initialize(const char *config_dir,
                           int64_t min_time_between_refresh_usec,
                           experiments_register_func_t register_func) {
  if (register_func == NULL)
    register_func = DefaultExperimentsRegisterFunc;

  experiments = new Experiments();
  return experiments->Initialize(config_dir, min_time_between_refresh_usec,
                                 register_func, {});
}

int experiments_is_initialized() {
  return experiments ? experiments->IsInitialized() : false;
}

int experiments_register(const char *name) {
  return experiments ? experiments->Register(name) : false;
}

int experiments_is_registered(const char *name) {
  return experiments ? experiments->IsRegistered(name) : false;
}

int experiments_get_num_of_registered_experiments() {
  return experiments ? experiments->GetNumOfRegisteredExperiments() : 0;
}

int experiments_is_enabled(const char *name) {
  return experiments ? experiments->IsEnabled(name) : false;
}
