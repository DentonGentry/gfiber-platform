#ifndef _H_LOGUPLOAD_CLIENT_UPLOAD_H_
#define _H_LOGUPLOAD_CLIENT_UPLOAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "kvextract.h"

int upload_file(const char* server_url, const char* target_name, char* data,
    ssize_t len, struct kvpair* kvpairs);

#ifdef __cplusplus
}
#endif

#endif  // _H_LOGUPLOAD_CLIENT_UPLOAD_H_
