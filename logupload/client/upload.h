#ifndef _H_LOGUPLOAD_CLIENT_UPLOAD_H_
#define _H_LOGUPLOAD_CLIENT_UPLOAD_H_

#include "kvextract.h"

int upload_file(const char* server_url, const char* target_name, char* data,
    ssize_t len, struct kvpair* kvpairs);

#endif  // _H_LOGUPLOAD_CLIENT_UPLOAD_H_
