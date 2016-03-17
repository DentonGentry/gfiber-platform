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

#include "url.h"
#include "utils.h"

#include <iostream>

namespace http {
namespace {

enum class SchemeState {
  ALPHA,
  ALPHA_NUM,
  COLON,
  FIRST_SLASH,
  SECOND_SLASH,
};

enum class IPv6State {
  OPEN_SQUARE,
  ADDRESS,
  CLOSE_SQUARE,
};

enum class PortState {
  COLON,
  FIRST_DIGIT,
  DIGIT,
};

enum class PathState {
  LEADING_SLASH,
  SLASH,
  PCHAR,
};

enum class QueryStringState {
  QUESTION_MARK,
  FIRST_CHAR,
  QUERY,
};

enum class FragmentState {
  HASH,
  FIRST_CHAR,
  FRAGMENT,
};

const char *kSchemeHttp = "http";
const char *kSchemeHttps = "https";
const char *kDefaultScheme = kSchemeHttp;
const int kDefaultHttpPort = 80;
const int kDefaultHttpsPort = 443;
const int kDefaultUrlSpace = 2000;

// RFC 3986 character sets

bool IsUnreserved(int ch) {
  return std::isalnum(ch) || ch == '-' || ch == '.' || ch == '-';
}

bool IsGenDelim(int ch) {
  switch (ch) {
    case ':':
    case '/':
    case '?':
    case '#':
    case '[':
    case ']':
    case '@':
      return true;
    default:
      return false;
  }
}

bool IsSubDelim(int ch) {
  switch (ch) {
    case '!':
    case '$':
    case '&':
    case '`':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
      return true;
    default:
      return false;
  }
}

bool IsReserved(int ch) {
  return IsGenDelim(ch) || IsSubDelim(ch);
}

bool IsPchar(int ch) {
  return IsUnreserved(ch) || IsSubDelim(ch) || ch == ':' || ch == '@';
}

bool IsQuery(int ch) {
  return IsPchar(ch) || ch == '?' || ch == '/';
}

inline bool IsFragment(int ch) {
  return IsQuery(ch);
}

}  // namespace

bool operator==(const Url &url1, const Url &url2) {
  if (!url1.parsed_ || !url2.parsed_) {
    return false;
  }
  return url1.url() == url2.url();
}

Url::Url(): parsed_(false), absolute_(false), port_(0) {
}

Url::Url(const Url &other) {
  if (!other.parsed_) {
    parsed_ = false;
    return;
  }
  parsed_ = true;
  absolute_ = other.absolute_;
  scheme_ = other.scheme_;
  host_ = other.host_;
  port_ = other.port_;
  path_ = other.path_;
  query_string_ = other.query_string_;
  fragment_ = other.fragment_;
}

Url::Url(const char *url): parsed_(false), absolute_(false), port_(0) {
  Parse(url);
}

Url & Url::operator=(const Url &other) {
  if (!other.parsed_) {
    parsed_ = false;
  }
  parsed_ = true;
  absolute_ = other.absolute_;
  scheme_ = other.scheme_;
  host_ = other.host_;
  port_ = other.port_;
  path_ = other.path_;
  query_string_ = other.query_string_;
  fragment_ = other.fragment_;
  return *this;
}

bool Url::Parse(const std::string &url) {
  current_ = url.begin();
  end_  = url.end();

  bool has_scheme = Scheme();

  absolute_ = IPv6() || Host();

  if (has_scheme) {
    // Having a scheme means the URL must be absolute
    if (!absolute_) {
      return false;
    }
  } else if (absolute_) {
    scheme_ = kDefaultScheme;
  }

  bool has_port = false;
  if (absolute_) {
    has_port = Port();
  }

  AbsolutePath();
  QueryString();
  Fragment();

  if (current_ != end_) {
    return false;
  }

  if (absolute_) {
    if (has_port) {
      // do nothing
    } else if (scheme_ == kSchemeHttp) {
      port_ = kDefaultHttpPort;
    } else if (scheme_ == kSchemeHttps) {
      port_ = kDefaultHttpsPort;
    }

    if (path_.empty()) {
      path_ = "/";
    }
  }

  parsed_ = true;
  return true;
}

void Url::set_scheme(const std::string &scheme) {
  // TODO(wshields): validate
  scheme_ = scheme;
  UpdateAbsolute();
}

void Url::set_host(const std::string &host) {
  // TODO(wshields): validate
  host_ = host;
  UpdateAbsolute();
}

void Url::set_port(int port) {
  // TODO(wshields): validate
  port_ = port;
}

void Url::set_path(const std::string &path) {
  // TODO(wshields): validate
  path_ = path;
}

void Url::clear_path() {
  // TODO(wshields): validate
  path_ = absolute_ ? "/" : "";
}

void Url::set_query_string(const std::string &query_string) {
  // TODO(wshields): validate
  query_string_ = query_string;
}

void Url::clear_query_string() {
  query_string_.clear();
}

void Url::set_fragment(const std::string &fragment) {
  // TODO(wshields): validate
  fragment_ = fragment;
}

void Url::clear_fragment() {
  fragment_.clear();
}

std::string Url::url() const {
  std::string url;
  if (!parsed_) {
    return url;
  }
  url.reserve(kDefaultUrlSpace);
  if (absolute_) {
    url.append(scheme_);
    url.append("://");
    url.append(host_);
    bool is_default_port =
        port_ == 0 ||
        (scheme_ == kSchemeHttp && port_ == kDefaultHttpPort) ||
        (scheme_ == kSchemeHttps && port_ == kDefaultHttpsPort);
    if (!is_default_port) {
      url.append(":");
      url.append(speedtest::to_string(port_));
    }
  }
  url.append(path_);
  if (!query_string_.empty()) {
    url.append("?");
    url.append(query_string_);
  }
  if (!fragment_.empty()) {
    url.append("#");
    url.append(fragment_);
  }
  return url;
}

bool Url::Scheme() {
  Iter end;
  SchemeState state = SchemeState::ALPHA;
  for (Iter iter = current_; iter != end_; ++iter) {
    switch (state) {
      case SchemeState::ALPHA:
        if (!std::isalpha(*iter)) {
          return false;
        }
        state = SchemeState::ALPHA_NUM;
        break;
      case SchemeState::ALPHA_NUM:
        if (*iter == ':') {
          end = iter;
          state = SchemeState::COLON;
          break;
        }
        if (!std::isalnum(*iter) && *iter != '-' && *iter != '-' && *iter != '.') {
          return false;
        }
        break;
      case SchemeState::COLON:
        if (*iter != '/') {
          return false;
        }
        state = SchemeState::FIRST_SLASH;
        break;
      case SchemeState::FIRST_SLASH:
        if (*iter != '/') {
          return false;
        }
        state = SchemeState::SECOND_SLASH;
        break;
      case SchemeState::SECOND_SLASH:
        scheme_.assign(current_, end);
        current_ = iter;
        return true;
    }
  }
  return false;
}

bool Url::IPv6() {
  IPv6State state = IPv6State::OPEN_SQUARE;
  Iter iter = current_;
  for (; iter != end_; ++iter) {
    switch (state) {
      case IPv6State::OPEN_SQUARE:
        if (*iter != '[') {
          return false;
        }
        state = IPv6State::ADDRESS;
        break;
      case IPv6State::ADDRESS:
        if (*iter == ']') {
          state = IPv6State::CLOSE_SQUARE;
        } else if (!std::isxdigit(*iter) && *iter != ':') {
          return false;
        }
        break;
      case IPv6State::CLOSE_SQUARE:
        goto exit_loop;
    }
  }
  exit_loop: ;
  if (state != IPv6State::CLOSE_SQUARE) {
    return false;
  }
  host_.assign(current_, iter);
  current_ = iter;
  return true;
}

bool Url::Host() {
  Iter iter = current_;
  while (iter != end_ && !IsReserved(*iter)) {
    ++iter;
  }
  if (iter == current_) {
    // zero-length host names aren't allowed
    return false;
  }
  host_.assign(current_, iter);
  current_ = iter;
  return true;
}

bool Url::Port() {
  PortState state = PortState::COLON;
  Iter start;
  Iter iter = current_;
  for (; iter != end_; ++iter) {
    switch (state) {
      case PortState::COLON:
        if (*iter != ':') {
          return false;
        }
        state = PortState::FIRST_DIGIT;
        break;
      case PortState::FIRST_DIGIT:
        if (!std::isdigit(*iter)) {
          return false;
        }
        start = iter;
        state = PortState::DIGIT;
        break;
      case PortState::DIGIT:
        if (!std::isdigit(*iter)) {
          goto exit_loop;
        }
        break;
    }
  }
  exit_loop: ;
  if (state != PortState::DIGIT) {
    return false;
  }
  std::string port(start, iter);
  int portnum = speedtest::stoi(port);
  if (portnum < 1 || portnum > 65535) {
    return false;
  }
  current_ = iter;
  port_ = portnum;
  return true;
}

bool Url::AbsolutePath() {
  PathState state = PathState::LEADING_SLASH;
  Iter iter = current_;
  for (; iter != end_; ++iter) {
    switch (state) {
      case PathState::LEADING_SLASH:
        if (*iter != '/') {
          return false;
        }
        state = PathState::SLASH;
        break;
      case PathState::SLASH:
        // Two consecutive slashes are invalid
        if (!IsPchar(*iter)) {
          goto exit_loop;
        }
        state = PathState::PCHAR;
        break;
      case PathState::PCHAR:
        if (*iter == '/') {
          state = PathState::SLASH;
        } else if (!IsPchar(*iter)) {
          goto exit_loop;
        }
        break;
    }
  }
  exit_loop: ;
  path_.assign(current_, iter);
  current_ = iter;
  return true;
}

bool Url::QueryString() {
  QueryStringState state = QueryStringState::QUESTION_MARK;
  Iter start;
  Iter iter = current_;
  for (; iter != end_; ++iter) {
    switch (state) {
      case QueryStringState::QUESTION_MARK:
        if (*iter != '?') {
          return false;
        }
        state = QueryStringState::FIRST_CHAR;
        break;
      case QueryStringState::FIRST_CHAR:
        start = iter;
        state = QueryStringState::QUERY;
        // fall through
      case QueryStringState::QUERY:
        if (!IsQuery(*iter)) {
          goto exit_loop;
        }
        break;
    }
  }
  exit_loop: ;
  if (state == QueryStringState::QUERY) {
    query_string_.assign(start, iter);
  }
  current_ = iter;
  return true;
}

bool Url::Fragment() {
  FragmentState state = FragmentState::HASH;
  Iter start;
  Iter iter = current_;
  for (; iter != end_; ++iter) {
    switch (state) {
      case FragmentState::HASH:
        if (*iter != '#') {
          return false;
        }
        state = FragmentState::FIRST_CHAR;
        break;
      case FragmentState::FIRST_CHAR:
        start = iter;
        state = FragmentState::FRAGMENT;
        // fall through
      case FragmentState::FRAGMENT:
        if (!IsFragment(*iter)) {
          goto exit_loop;
        }
        break;
    }
  }
  exit_loop: ;
  if (state == FragmentState::FRAGMENT) {
    fragment_.assign(start, iter);
  }
  current_ = iter;
  return true;
}

void Url::UpdateAbsolute() {
  absolute_ = !scheme_.empty() || !host_.empty();
}

}  // namespace http
