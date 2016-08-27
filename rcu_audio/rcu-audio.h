/*
 * Copyright 2016 Google Inc. All rights reserved.
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

#ifndef RCU_AUDIO_H
#define RCU_AUDIO_H

#define RCU_AUDIO_PATH "rcu_audio"

/* Return 1 if at least one second has passed since the
 * last successful call to pacing(). */
extern int pacing();

/* Return an AF_UNIX socket, or die trying. */
extern int get_socket_or_die();

#endif  /* RCU_AUDIO_H */
