#include <gtest/gtest.h>

#include "experiments.h"
#include "experiments_c_api_test.h"

#include <fcntl.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utils.h"

using namespace libexperiments_utils;  // NOLINT

int FailingExperimentsRegisterFunc(const char *name) {
  return false;
}

class ExperimentsTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    ASSERT_TRUE(realpath(".", root_path_));
    snprintf(test_folder_path_, sizeof(test_folder_path_), "%s/exps-XXXXXX",
             root_path_);
    char strerrbuf[1024] = {'\0'};
    ASSERT_TRUE(mkdtemp(test_folder_path_)) <<
        strerror_r(errno, strerrbuf, sizeof(strerrbuf)) << "(" << errno << ")";
    ASSERT_EQ(chdir(test_folder_path_), 0);
  }

  static void TearDownTestCase() {
    // change out of the test directory and remove it
    ASSERT_EQ(chdir(root_path_), 0);
    std::string cmd = StringPrintf("rm -r %s", test_folder_path_);
    ASSERT_EQ(0, system(cmd.c_str()));
  }

  bool CreateFile(const std::string &name) {
    int fd = open(name.c_str(), O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
      log_perror(errno, "Cannot create file '%s':", name.c_str());
      return false;
    } else {
      close(fd);
    }
    return true;
  }

  bool RenameFile(const std::string &from_name, const std::string &to_name) {
    if (rename(from_name.c_str(), to_name.c_str()) < 0) {
      log_perror(errno, "Cannot rename file '%s' to '%s':", from_name.c_str(),
                 to_name.c_str());
      return false;
    }
    return true;
  }

  bool DeleteFile(const std::string &name) {
    if (remove(name.c_str()) < 0) {
      log_perror(errno, "Cannot delete file '%s':", name.c_str());
      return false;
    }
    return true;
  }

  bool SwitchFromTo(Experiments *e, const std::string &name,
                    const std::string &from_ext, const std::string &to_ext) {
    std::string from_file = name + from_ext;
    std::string to_file = name + to_ext;
    if (file_exists(from_file.c_str())) {
      return RenameFile(from_file, to_file);
    } else {
      return CreateFile(to_file);
    }
  }

  bool SetActive(Experiments *e, const std::string &name) {
    return SwitchFromTo(e, name, ".inactive", ".active");
  }

  bool SetInactive(Experiments *e, const std::string &name) {
    return SwitchFromTo(e, name, ".active", ".inactive");
  }

  bool Remove(Experiments *e, const std::string &name) {
    std::string active_file = name + ".active";
    if (file_exists(active_file.c_str())) {
      if (!DeleteFile(active_file)) {
        return false;
      }
    }
    std::string inactive_file = name + ".inactive";
    if (file_exists(inactive_file.c_str())) {
      if (!DeleteFile(inactive_file)) {
        return false;
      }
    }
    return true;
  }

  static char root_path_[PATH_MAX];
  static char test_folder_path_[PATH_MAX];
};

char ExperimentsTest::test_folder_path_[PATH_MAX] = {0};
char ExperimentsTest::root_path_[PATH_MAX] = {0};


TEST_F(ExperimentsTest, InvalidConfigPath) {
  Experiments e;
  char invalid_path[1024];
  snprintf(invalid_path, sizeof(invalid_path), "%s/nope", test_folder_path_);
  ASSERT_FALSE(e.Initialize(invalid_path, 0, &DummyExperimentsRegisterFunc,
                            {"exp1"}));
}

TEST_F(ExperimentsTest, InvalidRegisterFunc) {
  Experiments e;
  ASSERT_FALSE(e.Initialize(test_folder_path_, 0, NULL, {"exp1"}));
}

TEST_F(ExperimentsTest, RegisterFuncFails) {
  Experiments e;
  ASSERT_FALSE(e.Initialize(test_folder_path_, 0,
                            &FailingExperimentsRegisterFunc, {"exp1"}));
}

TEST_F(ExperimentsTest, Register) {
  Experiments e;
  ASSERT_TRUE(e.Initialize(test_folder_path_, 0, &DummyExperimentsRegisterFunc,
                           {"exp1"}));
  EXPECT_TRUE(e.IsRegistered("exp1"));

  // add one more
  EXPECT_FALSE(e.IsRegistered("exp2"));
  EXPECT_TRUE(e.Register("exp2"));
  EXPECT_TRUE(e.IsRegistered("exp1"));
  EXPECT_TRUE(e.IsRegistered("exp2"));

  // repeated registration is ignored
  EXPECT_TRUE(e.Register("exp2"));
  EXPECT_TRUE(e.IsRegistered("exp1"));
  EXPECT_TRUE(e.IsRegistered("exp2"));

  // register vector
  EXPECT_FALSE(e.IsRegistered("exp3"));
  EXPECT_FALSE(e.IsRegistered("exp4"));
  EXPECT_FALSE(e.IsRegistered("exp5"));
  EXPECT_TRUE(e.Register({"exp3", "exp4", "exp5"}));
  EXPECT_TRUE(e.IsRegistered("exp1"));
  EXPECT_TRUE(e.IsRegistered("exp2"));
  EXPECT_TRUE(e.IsRegistered("exp3"));
  EXPECT_TRUE(e.IsRegistered("exp4"));
  EXPECT_TRUE(e.IsRegistered("exp5"));
}

TEST_F(ExperimentsTest, Single) {
  Experiments e;
  ASSERT_TRUE(e.Initialize(test_folder_path_, 0, &DummyExperimentsRegisterFunc,
                           {"exp1"}));
  EXPECT_FALSE(e.IsEnabled("exp1"));

  EXPECT_TRUE(SetActive(&e, "exp1"));
  EXPECT_TRUE(e.IsEnabled("exp1"));

  EXPECT_TRUE(SetInactive(&e, "exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));

  EXPECT_TRUE(SetActive(&e, "exp1"));
  EXPECT_TRUE(e.IsEnabled("exp1"));

  EXPECT_TRUE(Remove(&e, "exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
}

TEST_F(ExperimentsTest, Multiple) {
  Experiments e;
  ASSERT_TRUE(e.Initialize(test_folder_path_, 0, &DummyExperimentsRegisterFunc,
                           {"exp1", "exp2", "exp3"}));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));

  // activate exp1 - AII
  EXPECT_TRUE(SetActive(&e, "exp1"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));
  // activate exp2 - AAI
  EXPECT_TRUE(SetActive(&e, "exp2"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));
  // active exp3 - AAA
  EXPECT_TRUE(SetActive(&e, "exp3"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // inactivate exp2 - AIA
  EXPECT_TRUE(SetInactive(&e, "exp2"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // remove exp1 file - IIA
  EXPECT_TRUE(Remove(&e, "exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // re-activate exp2 - IAA
  EXPECT_TRUE(SetActive(&e, "exp2"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // inactivate exp1 (re-create file) - IAA
  EXPECT_TRUE(SetInactive(&e, "exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // remove all - III
  EXPECT_TRUE(Remove(&e, "exp1"));
  EXPECT_TRUE(Remove(&e, "exp2"));
  EXPECT_TRUE(Remove(&e, "exp3"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));
}

TEST_F(ExperimentsTest, TimeBetweenRefresh) {
  int64_t kMinTimeBetweenRefresh = secs_to_usecs(3);
  int64_t kTimeout =  secs_to_usecs(5);
  uint64_t start_time = us_elapse(0);
  Experiments e;
  ASSERT_TRUE(e.Initialize(test_folder_path_, kMinTimeBetweenRefresh,
                           &DummyExperimentsRegisterFunc, {"exp1"}));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_TRUE(SetActive(&e, "exp1"));

  // measure time until we see "exp1" active
  uint64_t duration = us_elapse(start_time);
  while (!e.IsEnabled("exp1") && duration < kTimeout) {
    us_sleep(100);
    duration = us_elapse(start_time);
  }

  EXPECT_GE(duration, kMinTimeBetweenRefresh) << "time:" << duration;
  EXPECT_LT(duration, kTimeout) << "time:" << duration;

  // clean up
  EXPECT_TRUE(Remove(&e, "exp1"));
}

TEST_F(ExperimentsTest, C_API_Test) {
  // returns false on all API functions until initialized is called
  EXPECT_FALSE(test_experiments_is_initialized());
  EXPECT_FALSE(test_experiments_register("exp1"));
  EXPECT_FALSE(test_experiments_is_registered("exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp1"));
  EXPECT_TRUE(SetActive(experiments, "exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp1"));
  EXPECT_TRUE(Remove(experiments, "exp1"));

  // initialize
  EXPECT_TRUE(test_experiments_initialize(test_folder_path_));
  EXPECT_TRUE(test_experiments_is_initialized());

  EXPECT_TRUE(test_experiments_register("exp1"));
  EXPECT_TRUE(test_experiments_is_registered("exp1"));

  EXPECT_FALSE(test_experiments_is_enabled("exp1"));
  EXPECT_TRUE(SetActive(experiments, "exp1"));
  EXPECT_TRUE(test_experiments_is_enabled("exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp2"));

  EXPECT_TRUE(SetInactive(experiments, "exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp1"));

  // clean up
  EXPECT_TRUE(Remove(experiments, "exp1"));
}
