// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef PLATFORM_SIGNING_READVERITY_H_
#define PLATFORM_SIGNING_READVERITY_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

  int readVerityParams(const char* fname);
  int readVerityHashSize(const char* fname);

#ifdef __cplusplus
}  // extern 'C'
#endif  /* __cplusplus */

#endif  // PLATFORM_SIGNING_READVERITY_H_
