#include "region.h"

#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <string>

// For some reason, the libjsoncpp package installs to /usr/include/jsoncpp/json
// instead of /usr{,/local}/include/json
#include <jsoncpp/json/json.h>

#include "errors.h"

namespace speedtest {
namespace {

bool AddUrl(const Json::Value &url_json, std::vector<http::Url> *urls) {
  if (!url_json.isString()) {
    return false;
  }
  http::Url url = http::Url(url_json.asString());
  if (!url.ok()) {
    return false;
  }
  urls->push_back(url);
  return true;
}

}  // namesapce

std::string DescribeRegion(const Region &region) {
  if (region.id.empty() && region.name.empty()) {
    return region.urls.front().url();
  }
  if (region.id.empty()) {
    return region.name;
  }
  if (region.name.empty()) {
    return region.id;
  }
  std::stringstream ss;
  ss << region.name << " (" << region.id << ")";
  return ss.str();
}

RegionResult LoadRegions(RegionOptions options) {
  RegionResult result;
  result.start_time = SystemTimeMicros();
  if (!options.request_factory) {
    result.status = Status(StatusCode::INVALID_ARGUMENT,
                           "request factory not set");
    result.end_time = SystemTimeMicros();
    return result;
  }

  if (!options.global) {
    if (options.verbose) {
      std::cout << "Explicit server list:\n";
      for (const auto &url : options.regional_urls) {
        std::cout << "  " << url.url() << "\n";
      }
    }
    for (const http::Url &url : options.regional_urls) {
      Region region;
      region.urls.emplace_back(url.url());
      result.regions.emplace_back(region);
    }
    result.status = Status::OK;
    result.end_time = SystemTimeMicros();
    return result;
  }

  http::Url config_url(options.global_url);
  config_url.set_path("/config");
  if (options.verbose) {
    std::cout << "Loading regions from " << config_url.url() << "\n";
  }
  http::Request::Ptr request = options.request_factory(config_url);
  request->set_url(config_url);
  request->set_timeout_millis(500);
  std::string json;
  CURLcode code = request->Get([&](void *data, size_t size){
    json.assign(static_cast<const char *>(data), size);
  });
  if (code != CURLE_OK) {
    result.status = Status(StatusCode::INTERNAL, http::ErrorString(code));
  } else {
    result.status = ParseRegions(json, &result.regions);
  }
  result.end_time = SystemTimeMicros();
  return result;
}

Status ParseRegions(const std::string &json, std::vector<Region> *regions) {
  if (!regions) {
    return Status(StatusCode::FAILED_PRECONDITION, "Regions is null");
  }

  Json::Reader reader;
  Json::Value root;
  if (!reader.parse(json, root, false)) {
    return Status(StatusCode::INVALID_ARGUMENT, "Failed to parse regions JSON");
  }

  if (!root.isMember("regions") || !root["regions"].isArray()) {
    return Status(StatusCode::INVALID_ARGUMENT, "no regions element found");
  }
  for (const auto &it : root["regions"]) {
    Region region;

    if (!it.isMember("id")) {
      return Status(StatusCode::INVALID_ARGUMENT, "Region missing id");
    }
    if (!it["id"].isString()) {
      return Status(StatusCode::INVALID_ARGUMENT, "Region id not a string");
    }
    region.id = it["id"].asString();

    if (it.isMember("name")) {
      if (!it["name"].isString()) {
        return Status(StatusCode::INVALID_ARGUMENT, "Region name not a string");
      }
      region.name = it["name"].asString();
    }

    if (!it.isMember("url")) {
      return Status(StatusCode::INVALID_ARGUMENT, "Region URL missing");
    }
    if (it["url"].isString()) {
      if (!AddUrl(it["url"], &region.urls)) {
        return Status(StatusCode::INVALID_ARGUMENT,
                      "Failed to parse region URL");
      }
    } else if (it["url"].isArray()) {
      for (const auto &url_it : it["url"]) {
        if (!AddUrl(url_it, &region.urls)) {
          return Status(StatusCode::INVALID_ARGUMENT,
                        "Failed to parse region URL");
        }
      }
      if (region.urls.empty()) {
        return Status(StatusCode::INVALID_ARGUMENT, "Region missing URLs");
      }
    } else {
      return Status(StatusCode::INVALID_ARGUMENT,
                    "Region URL not string or array");
    }

    regions->emplace_back(region);
  }
  return Status::OK;
}

}  // namespace
