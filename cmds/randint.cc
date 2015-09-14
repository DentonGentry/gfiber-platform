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

#include <iostream>
#include <random>

int rand_between(int min, int max)
{
  std::random_device rd;
  std::default_random_engine gen(rd());
  std::uniform_int_distribution<int> dist(min, max);
  return dist(gen);
}

void usage_and_die(const char *argv0)
{
  std::cerr << "Usage: " << argv0 << " [minval] <maxval>" << std::endl;
  exit(1);
}

int main(int argc, char **argv)
{
  int min = -1;
  int max = -1;

  switch (argc) {
  case 2:
    min = 0;
    max = atoi(argv[1]);
    break;
  case 3:
    min = atoi(argv[1]);
    max = atoi(argv[2]);
    break;
  default:
    usage_and_die(argv[0]);
  }

  std::cout << rand_between(min, max) << std::endl;
  return 0;
}
