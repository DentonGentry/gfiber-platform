// Copyright 2011 Google Inc. All Rights Reserved.
// Author: dgentry@google.com (Denny Gentry)


#include <stdio.h>
#include "gtest/gtest.h"
#include "hmx_upgrade_nvram.h"

int libupgrade_verbose = 1;

char* HMX_NVRAM_Read_Data = NULL;
DRV_Error HMX_NVRAM_Read(HMX_NVRAM_PARTITION_E partition,
                         unsigned char* pName, unsigned int offset,
                         unsigned char* pValue, unsigned int ulSize,
                         unsigned int* pLen) {
  if (HMX_NVRAM_Read_Data == NULL) {
    return DRV_ERR;
  } else {
    snprintf((char*)pValue, ulSize, "%s", HMX_NVRAM_Read_Data);
    *pLen = strlen(HMX_NVRAM_Read_Data);
    return DRV_OK;
  }
}

DRV_Error HMX_NVRAM_Write(HMX_NVRAM_PARTITION_E partition,
                         unsigned char* pName, unsigned int offset,
                         unsigned char* pValue, unsigned int ulSize) {
  HMX_NVRAM_Read_Data = (char*)malloc(ulSize);
  snprintf(HMX_NVRAM_Read_Data, sizeof(pValue), "%s", (char*)pValue);
  return DRV_OK;
}

DRV_Error HMX_NVRAM_Remove(HMX_NVRAM_PARTITION_E partition,
                           unsigned char* pName) {
  if (HMX_NVRAM_Read_Data == NULL) {
    return DRV_ERR;
  }
  HMX_NVRAM_Read_Data = NULL;
  return DRV_OK;
}

const char* HMX_NVRAM_GetField_Data = NULL;
DRV_Error HMX_NVRAM_GetField(NVRAM_FIELD_T field, unsigned int offset,
                             void* data, int nDataSize) {
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
                             void* data, int nDataSize) {
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

DRV_Error HMX_NVRAM_GetLength(tagNVRAM_FIELD partition, unsigned int* pLen) {
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
      HMX_NVRAM_Read_Data = NULL;
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
  EXPECT_STREQ("foo", format_nvram(HNVRAM_STRING, (unsigned char*)"foo", 3,
                                   out, sizeof(out)));
  EXPECT_STREQ("bar", format_nvram(HNVRAM_STRING, (unsigned char*)"bar", 3,
                                   out, sizeof(out)));

  unsigned char mac[6] = {0x11, 0x22, 0x03, 0x40, 0x55, 0xf6};
  EXPECT_STREQ("11:22:03:40:55:f6",
               format_nvram(HNVRAM_MAC, mac, sizeof(mac), out, sizeof(out)));

  const unsigned char in1[1] = {1};
  EXPECT_STREQ("1", format_nvram(HNVRAM_UINT8, in1, sizeof(in1),
                                 out, sizeof(out)));
  const unsigned char in254[1] = {0xfe};
  EXPECT_STREQ("254", format_nvram(HNVRAM_UINT8, in254, sizeof(in254),
                                   out, sizeof(out)));

  const unsigned char vers[] = {0x02, 0x01};
  EXPECT_STREQ("1.2", format_nvram(HNVRAM_HMXSWVERS, vers, sizeof(vers),
                                   out, sizeof(out)));

  const unsigned char gpn[] = {0x86, 0x0, 0x4, 0x0};
  EXPECT_STREQ("86000400", format_nvram(HNVRAM_GPN, gpn, sizeof(gpn),
                                        out, sizeof(out)));

  const unsigned char hex[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  EXPECT_STREQ("0123456789abcdef", format_nvram(
      HNVRAM_HEXSTRING, hex, sizeof(hex), out, sizeof(out)));
}

TEST_F(HnvramTest, TestParse) {
  char input[256];
  unsigned char output[256];
  unsigned int outlen = sizeof(output);

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

  outlen = sizeof(output);
  const char hex[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  snprintf(input, sizeof(input), "0123456789abcdef");
  EXPECT_TRUE(NULL != parse_nvram(HNVRAM_HEXSTRING, input, output, &outlen));
  EXPECT_EQ(8, outlen);
  EXPECT_EQ(0, memcmp(hex, output, outlen));
}

TEST_F(HnvramTest, TestGetNvramField) {
  EXPECT_EQ(NULL, get_nvram_field("nosuchfield"));
  EXPECT_EQ(NVRAM_FIELD_SYSTEM_ID, get_nvram_field("SYSTEM_ID")->nvram_type);
}

TEST_F(HnvramTest, TestReadFieldNvram) {
  char output[256];
  HMX_NVRAM_GetField_Data = "TestSystemId";
  EXPECT_STREQ("SYSTEM_ID=TestSystemId",
               read_nvram("SYSTEM_ID", output, sizeof(output), 0));
  EXPECT_STREQ("TestSystemId",
               read_nvram("SYSTEM_ID", output, sizeof(output), 1));
  HMX_NVRAM_GetField_Data = NULL;
  EXPECT_EQ(NULL, read_nvram("FAKE_SYSTEM_ID", output, sizeof(output), 1));
}

TEST_F(HnvramTest, TestReadVariableNvram) {
  char output[256];
  HMX_NVRAM_Read_Data = strdup("ABC123");
  EXPECT_STREQ("TEST_VARIABLE=ABC123",
               read_nvram("TEST_VARIABLE", output, sizeof(output), 0));
  EXPECT_STREQ("ABC123",
               read_nvram("TEST_VARIABLE", output, sizeof(output), 1));
  HMX_NVRAM_Read_Data = NULL;
  EXPECT_STREQ(NULL, read_nvram("TEST_VARIABLE", output, sizeof(output), 1));
}

TEST_F(HnvramTest, TestWriteFieldNvram) {
  // Type integer
  char* testdata = strdup("ACTIVATED_KERNEL_NUM=1");
  EXPECT_EQ(DRV_OK, write_nvram(testdata));
  EXPECT_EQ(0x01, *HMX_NVRAM_SetField_Data);
  EXPECT_EQ(1, HMX_NVRAM_SetField_Len);

  // Type string
  testdata = strdup("ACTIVATED_KERNEL_NAME=kernel1");
  EXPECT_EQ(DRV_OK, write_nvram(testdata));
  EXPECT_STREQ("kernel1", (char*)HMX_NVRAM_SetField_Data);
  EXPECT_EQ(7, HMX_NVRAM_SetField_Len);

  // Make sure it called SetField and not HMX_NVRAM_Write
  EXPECT_EQ (NULL, HMX_NVRAM_Read_Data);

  // Should fail trying to change value of non-exsting field
  testdata = strdup("FAKE_FIELD=abc123");
  EXPECT_NE(0, write_nvram(testdata));
  free(testdata);
}

TEST_F(HnvramTest, TestWriteVariableNvram) {
  char* key = strdup("TEST_FIELD");
  char* val = strdup("abc123");
  char* keyval = strdup("TEST_FIELD=abc123");

  // Fail to add new one without -n
  EXPECT_NE(0, write_nvram(strdup(keyval)));

  // Add new one successfully
  can_add_flag = 1;
  EXPECT_EQ(0, write_nvram(keyval));
  EXPECT_STREQ(val,HMX_NVRAM_Read_Data);

  // Should be able to read value
  char output[256];
  EXPECT_STREQ(val, read_nvram(key, output, sizeof(output), 1));

  char* val2 = strdup("987def");
  char* keyval2 = strdup("TEST_FIELD=987def");

  // Should be able to change value
  EXPECT_EQ(0, write_nvram(keyval2));
  EXPECT_STREQ(val2,HMX_NVRAM_Read_Data);

  free(key);
  free(val);
  free(keyval);
  free(val2);
  free(keyval2);
}

TEST_F(HnvramTest, TestClearNvram) {
  char* key = strdup("TEST_FIELD2");
  char* val = strdup("abc123");
  char* keyval = strdup("TEST_FIELD2=abc123");
  // No error if variable already cleared
  EXPECT_EQ(DRV_OK, clear_nvram(key));

  // Create new one
  can_add_flag = 1;
  EXPECT_EQ(0, write_nvram(keyval));
  EXPECT_STREQ(val, HMX_NVRAM_Read_Data);

  // Should be able to read value
  char output[256];
  EXPECT_STREQ(val, read_nvram(key, output, sizeof(output), 1));

  // Should be able to kill it
  EXPECT_EQ(DRV_OK, clear_nvram(key));

  // Should fail reading value
  EXPECT_STREQ(NULL, read_nvram(key, output, sizeof(output), 1));

  free(key);
  free(val);
  free(keyval);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
