#include <stdio.h>
#include <fcntl.h>
#include <zlib.h>
#include "gtest/gtest.h"
#include "utils.h"


static struct {
  const char* stripped;
  const char* original;
} rstrip_strings[] = {
  { "foobar", "foobar " },
  { "foobar", "foobar" },
  { "",       "" },
  { "foobar", "foobar \n" },
  { "foobar", "foobar\n" },
};

TEST(Utils, rstrip_test) {
  char buf[64];
  unsigned int i;
  for (i = 0; i < sizeof(rstrip_strings) / sizeof(rstrip_strings[0]); ++i) {
    strcpy(buf, rstrip_strings[i].original);
    rstrip(buf);
    EXPECT_STREQ(rstrip_strings[i].stripped, buf);
  }
}

// This tests both writing to and reading from a file at once.
TEST(Utils, read_write_file_success) {
  char buf[256];
  char tdir[32] = "utiltestXXXXXX";
  EXPECT_TRUE(mkdtemp(tdir) != NULL);
  char tfile[64];
  snprintf(tfile, sizeof(tfile), "%s/%s", tdir, "readfiletest");
  EXPECT_GT(write_to_file(tfile, "foobar\nmagic"), 0);
  EXPECT_NE(-1, read_file_as_string(tfile, buf, sizeof(buf)));
  remove(tfile);
  rmdir(tdir);
  EXPECT_STREQ("foobar\nmagic", buf);
}

TEST(Utils, read_file_as_string_fail) {
  char buf[256];
  EXPECT_EQ(-1, read_file_as_string("filedoesnotexist", buf, sizeof(buf)));
}

TEST(Utils, read_write_file_uint64_success) {
  uint64_t test_val = 123456789LL;
  char tdir[32] = "utiltestXXXXXX";
  EXPECT_TRUE(mkdtemp(tdir) != NULL);
  char tfile[64];
  snprintf(tfile, sizeof(tfile), "%s/%s", tdir, "uint64filetest");
  EXPECT_EQ(0, write_file_as_uint64(tfile, test_val));
  EXPECT_EQ(test_val, read_file_as_uint64(tfile));
  remove(tfile);
  rmdir(tdir);
}

TEST(Utils, read_file_as_uint64_noexist) {
  EXPECT_EQ(0, read_file_as_uint64("filedoesnotexist"));
}

TEST(Utils, path_exists_true) {
  char tdir[32] = "utiltestXXXXXX";
  EXPECT_TRUE(mkdtemp(tdir) != NULL);
  char tfile[64];
  snprintf(tfile, sizeof(tfile), "%s/%s", tdir, "existtest");
  write_to_file(tfile, "foo");
  EXPECT_EQ(1, path_exists(tfile));
  remove(tfile);
  rmdir(tdir);
}

TEST(Utils, path_exists_false) {
  EXPECT_EQ(0, path_exists("filedoesnotexist"));
}

TEST(Utils, parse_line_data_success) {
  struct line_data data;
  memset(&data, 0, sizeof(data));
  char buf[128] = "5,16,200,-;This is my log message of love\n";
  EXPECT_EQ(0, parse_line_data(buf, &data));
  EXPECT_STREQ("This is my log message of love\n", data.text);
  EXPECT_EQ(5, data.level);
  EXPECT_EQ(16, data.seq);
  EXPECT_EQ(200, data.ts_nsec);

  char buf2[128] =
      "2,33,54321,-;This is my log message of tests suck\ndictjunk\n";
  EXPECT_EQ(0, parse_line_data(buf2, &data));
  EXPECT_STREQ("This is my log message of tests suck\n", data.text);
  EXPECT_EQ(2, data.level);
  EXPECT_EQ(33, data.seq);
  EXPECT_EQ(54321, data.ts_nsec);
}

TEST(Utils, parse_line_data_failure) {
  struct line_data data;
  char buf[128] = "this is totally bad data";
  EXPECT_EQ(-1, parse_line_data(buf, &data));
  char buf2[128] = "1,2,3,-where's my semicolon";
  EXPECT_EQ(-1, parse_line_data(buf2, &data));
  char buf3[128] = "1,2,3,-;Where's my newline?";
  EXPECT_EQ(-1, parse_line_data(buf3, &data));
  char buf4[128] = "1,2 3 4 foo - where's my second comma?";
  EXPECT_EQ(-1, parse_line_data(buf4, &data));
}

TEST(Utils, deflate_in_place_test) {
  // We only test the success case because the failure cases shouldn't occur or
  // require incompressible data which we will never encounter in ASCII logs.
  z_stream zstrm;
  memset(&zstrm, 0, sizeof(zstrm));
  unsigned char random_data[16384];
  // Use only 7 bits of data so its closer to ASCII chars and will compress.
  for (unsigned int i = 0; i < sizeof(random_data); i++) {
    random_data[i] = random() % 128;
  }
  unsigned char backup_data[16384];
  memcpy(backup_data, random_data, sizeof(random_data));
  unsigned long comp_size = sizeof(random_data);
  EXPECT_EQ(Z_OK, deflate_inplace(&zstrm, random_data, sizeof(random_data),
      &comp_size));
  unsigned char decompressed[16384];
  unsigned long full_size = sizeof(decompressed);
  EXPECT_EQ(Z_OK, uncompress(decompressed, &full_size, random_data,
      comp_size));
  EXPECT_EQ(full_size, sizeof(random_data));
  EXPECT_EQ(0, memcmp(decompressed, backup_data, sizeof(backup_data)));
}
