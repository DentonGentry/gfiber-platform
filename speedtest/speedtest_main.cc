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

#include <cstdlib>
#include <iostream>
#include <memory>
#include "options.h"
#include "speedtest.h"

int main(int argc, char *argv[]) {
  speedtest::Options options;
  if (!speedtest::ParseOptions(argc, argv, &options) || options.usage) {
    speedtest::PrintUsage(argv[0]);
    std::exit(1);
  }
  if (options.verbose) {
    speedtest::PrintOptions(options);
  }
  speedtest::Speedtest speed(options);
  speed.Run();
  return 0;
}
