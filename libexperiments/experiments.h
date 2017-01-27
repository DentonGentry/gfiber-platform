#ifndef _LIBEXPERIMENTS_EXPERIMENTS_H
#define _LIBEXPERIMENTS_EXPERIMENTS_H

#include <inttypes.h>


// Implements a library that supports the Gfiber Experiments framework, as
// explained in the following doc: go/gfiber-experiments-framework.
//
// Both C and C++ (class) implementations are available.
//
// C++ example:
// ====================================
//   const char* kConfigFolderPath[] = "/fiber/config/experiments";
//   int64_t kMinTimeBetweenRefreshUs = 60 * 1000 * 1000;  // 60 secs
//   e = new Experiments();
//   if (!e->Initialize(kConfigFolderPath, kMinTimeBetweenRefreshUs,
//                      {"exp1", "exp2"})) {
//     // handle error case
//   }
//
//   // later in the code
//   if (e->IsEnabled("exp1")) {
//     // exp1 is enabled
//     [..]
//   }
//
// C example:
// ===================================
//   const char* kConfigFolderPath[] = "/fiber/config/experiments";
//   int64_t kMinTimeBetweenRefreshUs = 60 * 1000 * 1000;  // 60 secs
//   if (!experiments_initialize(kConfigFolderPath, kMinTimeBetweenRefreshUs,
//                               NULL);  // use default register function
//     // handle error case
//   }
//
//   experiments_register("exp1");
//   experiments_register("exp2");
//
//   // later in the code
//   if (experiments_is_enabled("exp1")) {
//     // exp1 is enabled
//     [..]
//   }


#ifdef __cplusplus
extern "C" {
#endif

// Function called when registering a new experiment.
// Returns non-zero (boolean true) for success, else 0 (boolean false).
typedef int (*experiments_register_func_t) (const char *name);

// Default experiment register function. Calls the shell script
// "register_experiment <name>".
int DefaultExperimentsRegisterFunc(const char *name);

// Dummy experiment register function. Just returns true.
int DummyExperimentsRegisterFunc(const char *name);

#ifdef __cplusplus
}
#endif


//
// C++ implementation
//
#ifdef __cplusplus

#include <atomic>
#include <mutex>  // NOLINT
#include <set>
#include <string>
#include <vector>


class Experiments {
 public:
  Experiments()
      : initialized_(false),
        min_time_between_refresh_usec_(0),
        last_time_refreshed_usec_(0) {}
  virtual ~Experiments() {}

  // Initializes the instance and registers any provided experiments. In detail:
  // * Sets the provided experiments config directory and register function and
  //   makes sure they are valid. If successful the instance is marked as
  //   initialized.
  // * Calls the register function for the provided experiment names.
  // * Scans the config folder to determine initial state of all registered
  //   experiments.
  // The min_time_between_refresh_usec values sets a lower boundary on how
  // often the config folder is scanned for updated experiment states.
  // Returns true if successful.
  bool Initialize(const std::string &config_dir,
                  int64_t min_time_between_refresh_usec,
                  experiments_register_func_t register_func,
                  const std::vector<std::string> &names_to_register);
  // Convenience version, using default experiments register function.
  bool Initialize(const std::string &config_dir,
                  int64_t min_time_between_refresh_usec,
                  const std::vector<std::string> &names_to_register) {
    return Initialize(config_dir, min_time_between_refresh_usec,
                      &DefaultExperimentsRegisterFunc, names_to_register);
  }

  bool IsInitialized() const { return initialized_; }

  // Registers the provided experiment(s).
  bool Register(const std::vector<std::string> &names);
  bool Register(const std::string &name) {
    std::vector<std::string> names{name};
    return Register(names);
  }

  int GetNumOfRegisteredExperiments() const {
    return registered_experiments_.size();
  }

  // Returns true if the given experiment is registered.
  bool IsRegistered(const std::string &name);

  // Returns true if the given experiment is active, else false. If the minimum
  // time between refreshes has passed, re-scans the config folder for updates
  // first.
  bool IsEnabled(const std::string &name);

 private:
  // Registers the given experiments. Unlocked version takes lock_ first.
  // Returns true if successful, else false.
  bool Register_Unlocked(const std::vector<std::string> &names);
  bool Register_Locked(const std::vector<std::string> &names);

  // Returns true if the given experiment is in the list of registered
  // experiments.
  bool IsInRegisteredList(const std::string &name) const {
    return registered_experiments_.find(name) != registered_experiments_.end();
  }

  // Refreshes all registered experiment states by scanning the config folder.
  void Refresh();

  // Updates the state of the given experiment by checking its file in the
  // config folder.
  void UpdateState(const std::string &name);

  // Returns true if the given experiment is in the list of enabled
  // experiments.
  bool IsInEnabledList(const std::string &name) {
    return enabled_experiments_.find(name) != enabled_experiments_.end();
  }

  std::atomic<bool> initialized_;
  std::mutex lock_;

  // Experiments config folder, containing the system-wide list of experiments.
  // An experiment is marked active if the folder contains the file named
  // "<experiment_name>.active".
  std::string config_dir_;

  // External function called to register an experiment.
  experiments_register_func_t register_func_;

  std::set<std::string> registered_experiments_;
  std::set<std::string> enabled_experiments_;

  // Minimum time between accessing the config folder to refresh the experiment
  // states. When set to 0 it refreshes on every call to IsEnabled().
  uint64_t min_time_between_refresh_usec_;
  uint64_t last_time_refreshed_usec_;
};

extern Experiments *experiments;

#endif  // __cplusplus


//
// C-based API
//

#ifdef __cplusplus
extern "C" {
#endif

// Creates and initializes the experiments object:
// * Sets the provided experiments config directory and register function.
// * Calls the register function for the provided experiment names.
// * Scans the config folder to determine initial state of all registered
//   experiments.
// The min_time_between_refresh_usec values sets a lower boundary on how often
// the config folder is scanned for updated experiment states. Set
// register_func to NULL to use the default register function
// (DefaultExperimentsRegisterFunc()).
// Returns non-zero (boolean true) if successful, 0 (boolean false) for error.
int experiments_initialize(const char *config_dir,
                            int64_t min_time_between_refresh_usec,
                            experiments_register_func_t register_func);

// Returns non-zero (boolean true) if the experiments object is initialized,
// else 0 (boolean false).
int experiments_is_initialized();

// Registers the provided experiment.
// Returns non-zero (boolean true) if successful, 0 (boolean false) for error.
int experiments_register(const char *name);

// Returns non-zero (boolean true) if the given experiment name is registered,
// else 0 (boolean false).
int experiments_is_registered(const char *name);

// Returns the number of experiments registered.
int experiments_get_num_of_registered_experiments();

// Returns non-zero (boolean true) if the given experiment is active, else 0
// (boolean false). If the minimum time between refreshes has passed, re-scans
// the config folder for updates first.
int experiments_is_enabled(const char *name);

#ifdef __cplusplus
}
#endif

#endif  // _LIBEXPERIMENTS_EXPERIMENTS_H
