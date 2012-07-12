// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)
#include <stdio.h>
#include "gtest/gtest.h"
#include "readverity.h"

namespace {

TEST(readverityTest, Success) {
  EXPECT_EQ(0, ::readverity("testdata/verityheader"));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
