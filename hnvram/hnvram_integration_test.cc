// Copyright 2016 Google Inc. All Rights Reserved.
// Author: germuth@google.com (Aaron Germuth)

// Tests external methods of hnvram_main end-to-end. No stubbing of lower-level
// methods (Black box testing)

#include <stdio.h>
#include "gtest/gtest.h"
#include "hmx_upgrade_nvram.h"

#define TEST_MAIN
#include "hnvram_main.c"
#include "hmx_test_base.cc"
#include "hmx_upgrade_nvram.c"
#include "hmx_upgrade_flash.c"

// Test constants
const char* name = "NEW_VAR";
const char* val = "ABCDEF";
const char* val2 = "ZZZZZZZZZ";
const int valLen = 6;
const int valLen2 = 9;

const char* fieldName = "MAC_ADDR_BT";
const char* fieldVal = "\x01\x02\x03\x04\x05\x06";
const char* fieldValStr = "01:02:03:04:05:06";
const char* fieldVal2 = "12:34:56:78:0a:bc";
const int fieldValLen = 6;

// Test parameters
const HMX_NVRAM_PARTITION_E partitions[] =
  {HMX_NVRAM_PARTITION_RO, HMX_NVRAM_PARTITION_RW};
HMX_NVRAM_PARTITION_E part;

class HnvramIntegrationTest : public HnvramTest,
  public ::testing::WithParamInterface<HMX_NVRAM_PARTITION_E> {
  public:
    HnvramIntegrationTest() {}
    virtual ~HnvramIntegrationTest() {}

    virtual void SetUp() {
      part = GetParam();

      libupgrade_verbose = 0;
      can_add_flag = 0;

      HnvramTest::SetUp();

      HMX_NVRAM_Init(hnvramFileName);
    }

    virtual void TearDown() {
      part = HMX_NVRAM_PARTITION_UNSPECIFIED;

      // clear dlists
      drv_NVRAM_Delete(HMX_NVRAM_PARTITION_RO, (unsigned char*)fieldName);
      drv_NVRAM_Delete(HMX_NVRAM_PARTITION_RW, (unsigned char*)name);
      drv_NVRAM_Delete(HMX_NVRAM_PARTITION_RO, (unsigned char*)name);

      HnvramTest::TearDown();
    }
};

TEST_P(HnvramIntegrationTest, TestWriteNvramNew) {
  // Should fail without can_add
  EXPECT_EQ(-1, write_nvram_new(name, val, part));

  // Should fail to parse
  can_add_flag = 1;
  char valLarge[NVRAM_MAX_DATA + 1];
  memset(valLarge, 1, sizeof(valLarge));
  EXPECT_EQ(-2, write_nvram_new(name, valLarge, part));

  // Should fail cleanly with bad partition
  HMX_NVRAM_Init("/tmp/");
  EXPECT_EQ(-3, write_nvram_new(name, val, part));

  // Read back writes
  HMX_NVRAM_Init(hnvramFileName);
  unsigned char read[255];
  unsigned int readLen = 0;
  EXPECT_EQ(0, write_nvram_new(name, val, part));
  EXPECT_EQ(DRV_OK,
            HMX_NVRAM_Read(part, (unsigned char*)name, 0,
                           read, sizeof(read), &readLen));
  EXPECT_EQ(0, memcmp(val, read, valLen));
  EXPECT_EQ(valLen, readLen);
}

TEST_P(HnvramIntegrationTest, TestWriteNvram) {
  // Should fail with large val
  char valLarge[NVRAM_MAX_DATA + 1];
  memset(valLarge, 1, sizeof(valLarge));
  EXPECT_EQ(-1, write_nvram(name, valLarge, part));

  // Failure to parse
  EXPECT_EQ(-2, write_nvram(fieldName, "not-proper-mac-addr", part));

  // Variable doesn't already exist
  EXPECT_EQ(-3, write_nvram(name, val, part));

  // Variable exists in wrong partition
  can_add_flag = 1;
  EXPECT_EQ(0, write_nvram_new(name, val, part));
  EXPECT_EQ(-4, write_nvram(name, val, HMX_NVRAM_PARTITION_W_RAWFS));

  // Fail cleanly from lower-level write
  HMX_NVRAM_Init("/tmp/");
  EXPECT_EQ(-5, write_nvram(name, val, part));
  HMX_NVRAM_Init(hnvramFileName);

  // Try to specify partition with a field variable
  EXPECT_EQ(0, write_nvram_new(fieldName, fieldVal, part));
  HMX_NVRAM_Init(hnvramFileName);
  EXPECT_EQ(-6, write_nvram(fieldName, fieldVal2, part));

  // Failure from lower-level write w/field
  HMX_NVRAM_Init("/tmp/");
  char out[255];
  EXPECT_EQ(-7, write_nvram(fieldName, fieldValStr,
                        HMX_NVRAM_PARTITION_UNSPECIFIED));
  HMX_NVRAM_Init(hnvramFileName);

  // Read back val after changing val
  EXPECT_EQ(0, write_nvram(name, val2, part));
  unsigned char read[255];
  unsigned int readLen = 0;
  EXPECT_EQ(DRV_OK, HMX_NVRAM_Read(part, (unsigned char*)name, 0,
                           read, sizeof(read), &readLen));
  EXPECT_EQ(0, memcmp(read, val2, readLen));
  EXPECT_EQ(readLen, valLen2);
}

TEST_P(HnvramIntegrationTest, TestClearNvram) {
  // Delete non-existing variable
  HMX_NVRAM_Init(hnvramFileName);
  EXPECT_EQ(DRV_OK, clear_nvram(name));

  can_add_flag = 1;
  EXPECT_EQ(0, write_nvram_new(name, val, part));

  // No hnvram partition
  HMX_NVRAM_Init("/tmp/");
  EXPECT_EQ(DRV_ERR, clear_nvram(name));

  // Delete Existing
  HMX_NVRAM_Init(hnvramFileName);
  EXPECT_EQ(DRV_OK, clear_nvram(name));
}

TEST_P(HnvramIntegrationTest, TestReadNvram) {
  char readR[255];
  memset(readR, 56, 30);
  HMX_NVRAM_PARTITION_E part_used;
  EXPECT_TRUE(NULL == read_nvram(fieldName, readR, sizeof(readR), 0, &part_used));

  // No variable to find
  EXPECT_EQ(NULL, read_nvram(name, readR, sizeof(readR), 0, &part_used));

  // Find field
  can_add_flag = 1;
  EXPECT_EQ(0, write_nvram_new(fieldName, fieldVal, HMX_NVRAM_PARTITION_RO));
  EXPECT_FALSE(NULL == read_nvram(fieldName, readR, sizeof(readR), 1, &part_used));
  EXPECT_EQ(0, memcmp(readR, fieldValStr, 18));
  EXPECT_EQ(part_used, HMX_NVRAM_PARTITION_RO);

  // Find variable
  EXPECT_EQ(0, write_nvram_new(name, val, part));
  EXPECT_FALSE(NULL == read_nvram(name, readR, sizeof(readR), 1, &part_used));
  EXPECT_EQ(0, memcmp(readR, val, valLen));
  EXPECT_EQ(part_used, part);
}

TEST_P(HnvramIntegrationTest, TestInitNvram) {
  char readR[255];
  HMX_NVRAM_PARTITION_E part_used;

  // Set envvar to bad file
  EXPECT_EQ(0, setenv("HNVRAM_LOCATION", "/tmp/", 1));
  EXPECT_EQ(DRV_OK, init_nvram());

  // Should fail to read
  EXPECT_TRUE(NULL == read_nvram(name, readR, sizeof(readR), 1, &part_used));

  // Set envvar to proper, empty file
  EXPECT_EQ(0, setenv("HNVRAM_LOCATION", hnvramFileName, 1));
  EXPECT_EQ(DRV_OK, init_nvram());

  // Write and read it back
  can_add_flag = 1;
  EXPECT_EQ(0, write_nvram_new(name, val, part));

  EXPECT_FALSE(NULL == read_nvram(name, readR, sizeof(readR), 1, &part_used));
  EXPECT_EQ(0, memcmp(readR, val, valLen));
  EXPECT_EQ(part_used, part);
}

INSTANTIATE_TEST_CASE_P(TryAllPartitions, HnvramIntegrationTest,
                        ::testing::ValuesIn(partitions));

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new HnvramEnvironment);
  return RUN_ALL_TESTS();
}
