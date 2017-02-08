#include <gtest/gtest.h>

#include "experiments.h"
#include "experiments_c_api_test.h"

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

  bool SetRequested(const std::string &exp_name) {
    remove((exp_name + ".unrequested").c_str());
    return touch_file((exp_name + ".requested").c_str());
  }

  bool SetUnrequested(const std::string &exp_name) {
    remove((exp_name + ".requested").c_str());
    return touch_file((exp_name + ".unrequested").c_str());
  }

  void Remove(const std::string &exp_name) {
    remove((exp_name + ".unrequested").c_str());
    remove((exp_name + ".requested").c_str());
    remove((exp_name + ".active").c_str());
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
  EXPECT_EQ(1, e.GetNumOfRegisteredExperiments());

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
  EXPECT_EQ(1, e.GetNumOfRegisteredExperiments());

  EXPECT_TRUE(SetRequested("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp1"));

  EXPECT_TRUE(SetUnrequested("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));

  EXPECT_TRUE(SetRequested("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp1"));

  EXPECT_TRUE(SetUnrequested("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));

  // clean up
  Remove("exp1");
}

TEST_F(ExperimentsTest, Multiple) {
  Experiments e;
  ASSERT_TRUE(e.Initialize(test_folder_path_, 0, &DummyExperimentsRegisterFunc,
                           {"exp1", "exp2", "exp3"}));
  EXPECT_EQ(3, e.GetNumOfRegisteredExperiments());
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));

  // activate exp1 - AII
  EXPECT_TRUE(SetRequested("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));
  // activate exp2 - AAI
  EXPECT_TRUE(SetRequested("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));
  // active exp3 - AAA
  EXPECT_TRUE(SetRequested("exp3"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // inactivate exp2 - AIA
  EXPECT_TRUE(SetUnrequested("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // inactivate exp1 file - IIA
  EXPECT_TRUE(SetUnrequested("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // re-activate exp2 - IAA
  EXPECT_TRUE(SetRequested("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // inactivate exp1 (re-create file) - IAA
  EXPECT_TRUE(SetUnrequested("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_TRUE(e.IsEnabled("exp2"));
  EXPECT_TRUE(e.IsEnabled("exp3"));
  // inactivate all - III
  EXPECT_TRUE(SetUnrequested("exp1"));
  EXPECT_TRUE(SetUnrequested("exp2"));
  EXPECT_TRUE(SetUnrequested("exp3"));
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_FALSE(e.IsEnabled("exp2"));
  EXPECT_FALSE(e.IsEnabled("exp3"));

  // clean up
  Remove("exp1");
  Remove("exp2");
  Remove("exp3");
}

TEST_F(ExperimentsTest, TimeBetweenRefresh) {
  int64_t kMinTimeBetweenRefresh = secs_to_usecs(3);
  int64_t kTimeout =  secs_to_usecs(5);
  uint64_t start_time = us_elapse(0);
  Experiments e;
  ASSERT_TRUE(e.Initialize(test_folder_path_, kMinTimeBetweenRefresh,
                           &DummyExperimentsRegisterFunc, {"exp1"}));
  EXPECT_EQ(1, e.GetNumOfRegisteredExperiments());
  EXPECT_FALSE(e.IsEnabled("exp1"));
  EXPECT_TRUE(SetRequested("exp1"));

  // measure time until we see "exp1" active
  uint64_t duration = us_elapse(start_time);
  while (!e.IsEnabled("exp1") && duration < kTimeout) {
    us_sleep(100);
    duration = us_elapse(start_time);
  }

  EXPECT_GE(duration, kMinTimeBetweenRefresh) << "time:" << duration;
  EXPECT_LT(duration, kTimeout) << "time:" << duration;

  // clean up
  Remove("exp1");
}

TEST_F(ExperimentsTest, C_API_Test) {
  // returns false on all API functions until initialized is called
  EXPECT_FALSE(test_experiments_is_initialized());
  EXPECT_FALSE(test_experiments_register("exp1"));
  EXPECT_FALSE(test_experiments_is_registered("exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp1"));
  EXPECT_TRUE(SetRequested("exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp1"));
  EXPECT_TRUE(SetUnrequested("exp1"));

  // initialize
  EXPECT_TRUE(test_experiments_initialize(test_folder_path_));
  EXPECT_TRUE(test_experiments_is_initialized());
  EXPECT_EQ(0, experiments_get_num_of_registered_experiments());

  EXPECT_TRUE(test_experiments_register("exp1"));
  EXPECT_TRUE(test_experiments_is_registered("exp1"));
  EXPECT_EQ(1, experiments_get_num_of_registered_experiments());

  EXPECT_FALSE(test_experiments_is_enabled("exp1"));
  EXPECT_TRUE(SetRequested("exp1"));
  EXPECT_TRUE(test_experiments_is_enabled("exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp2"));

  EXPECT_TRUE(SetUnrequested("exp1"));
  EXPECT_FALSE(test_experiments_is_enabled("exp1"));

  // clean up
  EXPECT_TRUE(SetUnrequested("exp1"));
}
