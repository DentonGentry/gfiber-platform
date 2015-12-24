#include <gtest/gtest.h>

#include "request.h"

#include <memory>
#include "curl_env.h"

namespace http {
namespace {

class RequestTest : public testing::Test {
 protected:
  std::shared_ptr<CurlEnv> env;
  std::unique_ptr<Request> request;

  void SetUp() override {
    env = std::make_shared<CurlEnv>();
    request = env->NewRequest();
  }

  void VerifyQueryString(const char *expected,
                         Request::QueryStringParams params) {
    request->params() = params;
    request->UpdateUrl();
    EXPECT_EQ(expected, request->url().query_string());
  }

  void VerifyUrl(const char *expected,
                 const char *url,
                 Request::QueryStringParams params) {
    request->set_url(Url(url));
    request->params() = params;
    request->UpdateUrl();
    EXPECT_EQ(expected, request->url().url());
  }
};

TEST_F(RequestTest, QueryString_Empty_Ok) {
  VerifyQueryString("", {});
}

TEST_F(RequestTest, QueryString_SingleParam_Ok) {
  VerifyQueryString("abc=def", {{"abc", "def"}});
}

TEST_F(RequestTest, QueryString_MultipleValues_Ok) {
  VerifyQueryString("abc=def&abc=ghi", {{"abc", "def"}, {"abc", "ghi"}});
}

TEST_F(RequestTest, QueryString_TwoParams_Ok) {
  VerifyQueryString("abc=def&def=ghi", {{"abc", "def"}, {"def", "ghi"}});
}

TEST_F(RequestTest, QueryString_Escape_Ok) {
  VerifyQueryString("%2B%3D%26%20=%20%26%3D%2B", {{"+=& ", " &=+"}});
}

TEST_F(RequestTest, Url_Empty_Ok) {
  VerifyUrl("", "", {});
}

TEST_F(RequestTest, Url_NoParams_Ok) {
  VerifyUrl("http://example.com/", "http://example.com", {});
}

TEST_F(RequestTest, Url_OneParam_Ok) {
  VerifyUrl("http://example.com/?abc=def",
            "http://example.com",
            {{"abc", "def"}});
}

TEST_F(RequestTest, Url_TwoParams_Ok) {
  VerifyUrl("http://example.com/?abc=def&def=ghi",
            "http://example.com",
            {{"abc", "def"}, {"def", "ghi"}});
}

TEST_F(RequestTest, Url_OneParamTwoValues_Ok) {
  VerifyUrl("http://example.com/?abc=def&abc=ghi",
            "http://example.com",
            {{"abc", "def"}, {"abc", "ghi"}});
}

TEST_F(RequestTest, Url_EscapeParam_Ok) {
  VerifyUrl("http://example.com/?%2B%3D%26%20=%20%26%3D%2B",
            "http://example.com",
            {{"+=& ", " &=+"}});
}

}  // namespace
}  // namespace http
