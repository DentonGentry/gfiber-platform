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

#include "transfer_runner.h"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace speedtest {
namespace {

const int kDefaultIntervalMillis = 200;

}  // namespace

double GetShortEma(std::vector<Bucket> *buckets, int num_buckets) {
  if (buckets->empty() || num_buckets <= 0) {
    return 0.0;
  }
  const Bucket &last_bucket = buckets->back();
  double percent = 2.0d / (num_buckets + 1);
  return GetSimpleAverage(buckets, 1) * percent +
      last_bucket.short_megabits * (1 - percent);
}

double GetLongEma(std::vector<Bucket> *buckets, int num_buckets) {
  if (buckets->empty() || num_buckets <= 0) {
    return 0.0;
  }
  const Bucket &last_bucket = buckets->back();
  double percent = 2.0d / (num_buckets + 1);
  return GetSimpleAverage(buckets, 1) * percent +
      last_bucket.long_megabits * (1 - percent);
}

double GetSimpleAverage(std::vector<Bucket> *buckets, int num_buckets) {
  if (buckets->empty() || num_buckets <= 0) {
    return 0.0;
  }
  int end_index = buckets->size() - 1;
  int start_index = std::max(0, end_index - num_buckets);
  const Bucket &end = (*buckets)[end_index];
  const Bucket &start = (*buckets)[start_index];
  return ToMegabits(end.total_bytes - start.total_bytes,
                    end.start_time - start.start_time);
}

}  // namespace
