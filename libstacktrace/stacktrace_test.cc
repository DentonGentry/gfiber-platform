#include "stacktrace.h"
#include "gtest/gtest.h"

TEST(StacktraceTest, Output) {
  struct format_uint_test {
    int line;
    unsigned int uint;
    const char *expected_str;
  } test_arr[] = {
    {__LINE__, 0, "0" },
    {__LINE__, 1, "1" },
    {__LINE__, 12, "12" },
    {__LINE__, 123, "123" },
    {__LINE__, 1234, "1234" },
    {__LINE__, 12345, "12345" },
    {__LINE__, 22865, "22865" },
    {__LINE__, 54321, "54321" },
  };

  for (const auto &test_item : test_arr) {
    EXPECT_STREQ(test_item.expected_str, format_uint(test_item.uint))
        << test_item.line;
  }
}
