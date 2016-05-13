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

#ifndef SPEEDTEST_RESULT_H
#define SPEEDTEST_RESULT_H

#include <jsoncpp/json/json.h>
#include "config.h"
#include "find_nearest.h"
#include "init.h"
#include "ping.h"
#include "speedtest.h"
#include "transfer_runner.h"

namespace speedtest {

void PopulateParameters(Json::Value &json, const Config &config);
void PopulateConfigResult(Json::Value &json,
                          const ConfigResult &config_result);
void PopulateFindNearest(Json::Value &json,
                         const FindNearest::Result &find_nearest);
void PopulateInitResult(Json::Value &json,
                        const Init::Result &init_result);
void PopulateTransfer(Json::Value &json,
                      const TransferResult &transfer_result);
void PopulatePingResult(Json::Value &json, const Ping::Result &ping_result);
void PopulateSpeedtest(Json::Value &json,
                       const Speedtest::Result &speedtest_result);

}  // namespace speedtest

#endif  // SPEEDTEST_RESULT_H
