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
#ifndef _DEVMAP_H_
#define _DEVMAP_H_

#include <stddef.h>

// devmap_create creates a device mapper target.
// Returns 0 on success or -1 on error.
int devmap_create(const char* name, int start, int size, const char* params);

// devmap_remove removes a device mapper target.
// Returns 0 on success or -1 on error.
int devmap_remove(const char* name);

// devmap_make_params constructs dm-crypt target parameters.
// Caller must free returned string.
char* devmap_make_params(const char* alg, void* key, size_t key_size, const char* blk_dev);

#endif /* _DEVMAP_H_ */
