/*
 * Copyright 2015 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _TPM_H_
#define _TPM_H_

#include <stddef.h>

typedef struct tpm_handle* tpm_handle_t;

// tpm_open opens the TPM device/service.
tpm_handle_t tpm_open();

// tpm_close closes the TPM device/service.
void tpm_close(tpm_handle_t h);

// tpm_read_random reads count bytes of random data.
// Current implmentation uses /dev/urandom.
// Returns 0 on success or -1 on error.
int tpm_read_random(void* buf, size_t count);

// tpm_decrypt decrypts data using TPM's internal key.
// Returns 0 on success or -1 on error.
int tpm_decrypt(
    tpm_handle_t h,
    void* input, size_t input_size,
    void** output, size_t* output_size);

// tpm_decrypt encrypts data using TPM's internal key.
// Returns 0 on success or -1 on error.
int tpm_encrypt(
    tpm_handle_t h,
    void* input, size_t input_size,
    void** output, size_t* output_size);

#endif /* _TPM_H_ */
