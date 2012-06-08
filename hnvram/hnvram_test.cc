// Copyright 2011 Google Inc. All Rights Reserved.
// Author: dgentry@google.com (Denny Gentry)


#include <stdio.h>
#include "gtest/gtest.h"
#include "hmx_upgrade_nvram.h"

int libupgrade_verbose = 1;

const char* HMX_NVRAM_GetField_Data = NULL;
DRV_Error HMX_NVRAM_GetField(NVRAM_FIELD_T field, unsigned int offset,
                             void *data, int nDataSize) {
  if (HMX_NVRAM_GetField_Data == NULL) {
    return DRV_ERR;
  } else {
    snprintf((char*)data, nDataSize, "%s", HMX_NVRAM_GetField_Data);
    return DRV_OK;
  }
}

unsigned char* HMX_NVRAM_SetField_Data = NULL;
int HMX_NVRAM_SetField_Len = -1;
DRV_Error HMX_NVRAM_SetField_Return = DRV_OK;
DRV_Error HMX_NVRAM_SetField(NVRAM_FIELD_T field, unsigned int offset,
                             void *data, int nDataSize) {
  HMX_NVRAM_SetField_Data = (unsigned char*)malloc(nDataSize);
  memcpy(HMX_NVRAM_SetField_Data, data, nDataSize);
  HMX_NVRAM_SetField_Len = nDataSize;
  return HMX_NVRAM_SetField_Return;
}

DRV_Error HMX_NVRAM_Init(void) {
  return DRV_OK;
}

DRV_Error HMX_NVRAM_Dir(void) {
  return DRV_OK;
}

DRV_Error HMX_NVRAM_GetLength(tagNVRAM_FIELD partition, int *pLen) {
  *pLen = HMX_NVRAM_SetField_Len;
  return DRV_OK;
}


#define TEST_MAIN
#include "hnvram_main.c"


class HnvramTest : public ::testing::Test {
  public:
    HnvramTest() {}
    virtual ~HnvramTest() {}

    virtual void SetUp() {
      HMX_NVRAM_GetField_Data = NULL;
      HMX_NVRAM_SetField_Data = NULL;
      HMX_NVRAM_SetField_Len = -1;
      HMX_NVRAM_SetField_Return = DRV_OK;
    }

    virtual void TearDown() {
      if (HMX_NVRAM_SetField_Data != NULL) {
        free(HMX_NVRAM_SetField_Data);
        HMX_NVRAM_SetField_Data = NULL;
      }
    }
};

TEST_F(HnvramTest, TestFormat) {
  char out[256];
  EXPECT_STREQ("foo", format_nvram(HNVRAM_STRING, "foo", out, sizeof(out)));
  EXPECT_STREQ("bar", format_nvram(HNVRAM_STRING, "bar", out, sizeof(out)));

  char mac[6] = {0x11, 0x22, 0x03, 0x40, 0x55, 0xf6};
  EXPECT_STREQ("11:22:03:40:55:f6",
               format_nvram(HNVRAM_MAC, mac, out, sizeof(out)));

  const char in1[1] = {1};
  EXPECT_STREQ("1", format_nvram(HNVRAM_UINT8, in1, out, sizeof(out)));
  const char in254[1] = {0xfe};
  EXPECT_STREQ("254", format_nvram(HNVRAM_UINT8, in254, out, sizeof(out)));

  const char vers[] = {0x02, 0x01};
  EXPECT_STREQ("1.2", format_nvram(HNVRAM_HMXSWVERS, vers, out, sizeof(out)));

  const char gpn[] = {0x86, 0x0, 0x4, 0x0};
  EXPECT_STREQ("86000400", format_nvram(HNVRAM_GPN, gpn, out, sizeof(out)));
}

TEST_F(HnvramTest, TestGetNvramField) {
  EXPECT_EQ(NULL, get_nvram_field("nosuchfield"));
  EXPECT_EQ(NVRAM_FIELD_SYSTEM_ID, get_nvram_field("SYSTEM_ID")->nvram_type);
}

TEST_F(HnvramTest, TestReadNvram) {
  char output[256];
  HMX_NVRAM_GetField_Data = "TestSystemId";
  EXPECT_STREQ("SYSTEM_ID=TestSystemId",
               read_nvram("SYSTEM_ID", output, sizeof(output), 0));
}

TEST_F(HnvramTest, TestParse) {
  char input[256];
  unsigned char output[256];
  int outlen = sizeof(output);

  snprintf(input, sizeof(input), "This is a test.");
  EXPECT_TRUE(NULL != parse_nvram(HNVRAM_STRING, input, output, &outlen));
  EXPECT_STREQ(input, (char*)output);
  EXPECT_EQ(outlen, strlen(input));

  outlen = sizeof(output);
  unsigned char expected_mac[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
  EXPECT_TRUE(NULL != parse_nvram(HNVRAM_MAC, "12:34:56:78:9a:bc",
                                  output, &outlen));
  EXPECT_EQ(6, outlen);
  EXPECT_EQ(0, memcmp(expected_mac, output, outlen));

  outlen = sizeof(output);
  const char expected_in9 = '9';
  input[0] = 9;
  EXPECT_TRUE(NULL != parse_nvram(HNVRAM_UINT8, &expected_in9,
                                  output, &outlen));
  EXPECT_EQ(1, outlen);
  EXPECT_EQ(output[0], 9);

  outlen = sizeof(output);
  const char vers[] = {0x01, 0x02};
  snprintf(input, sizeof(input), "2.1");
  EXPECT_TRUE(NULL != parse_nvram(HNVRAM_HMXSWVERS, input, output, &outlen));
  EXPECT_EQ(2, outlen);
  EXPECT_EQ(0, memcmp(vers, output, outlen));

  outlen = sizeof(output);
  const char gpn[] = {0x86, 0x0, 0x4, 0x0};
  snprintf(input, sizeof(input), "86000400");
  EXPECT_TRUE(NULL != parse_nvram(HNVRAM_GPN, input, output, &outlen));
  EXPECT_EQ(4, outlen);
  EXPECT_EQ(0, memcmp(gpn, output, outlen));
}

TEST_F(HnvramTest, TestWriteNvram) {
  char* testdata = strdup("ACTIVATED_KERNEL_NUM=1");
  EXPECT_EQ(DRV_OK, write_nvram(testdata));
  unsigned char expected[] = {0x01};
  EXPECT_EQ(0, memcmp(HMX_NVRAM_SetField_Data, expected, sizeof(expected)));
  EXPECT_EQ(1, HMX_NVRAM_SetField_Len);
  free(testdata);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
