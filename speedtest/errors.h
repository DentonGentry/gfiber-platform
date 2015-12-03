#ifndef HTTP_ERRORS_H
#define HTTP_ERRORS_H

#include <curl/curl.h>

namespace http {

const char *ErrorString(CURLcode error_code);

}  // namespace http

#endif  // HTTP_ERRORS_H
