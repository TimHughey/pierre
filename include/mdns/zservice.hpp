// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/types.hpp"

#include <algorithm>
#include <any>
#include <boost/algorithm/string.hpp>
#include <future>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <string_view>
#include <vector>

namespace pierre {

namespace zc {
class Txt;
using TxtList = std::vector<Txt>;

class Txt {
public:
  Txt(ccs key_ccs, ccs val_ccs) : key_string(key_ccs) {
    string val(val_ccs);

    if (std::ranges::all_of(val.begin(), val.end(),
                            [](char c) { return (c >= '0') && (c <= '9'); })) {
      is_number = true;
      val_any.emplace<uint32_t>(std::stol(val));
    } else {
      val_any.emplace<string>(val);
    }
  }

  const string &key() const { return key_string; }
  bool number() const { return is_number; }
  template <typename T = string> const T &val() const { return std::any_cast<const T &>(val_any); }

private:
  string key_string;
  std::any val_any;
  bool is_number = false;
};

} // namespace zc

class ZeroConf {
public:
  struct Details {
    ccs domain;
    ccs hostname;
    ccs name_net;
    ccs address;
    ccs type;
    uint16_t port;
    ccs protocol;
    zc::TxtList txt_list;
  };

public:
  ZeroConf() = default;
  ZeroConf(Details d) noexcept;

  const auto findTxtByKey(csv key) const noexcept {
    return std::ranges::find_if(_txt_list.begin(), _txt_list.end(),
                                [=](auto &txt) { return txt.key() == key; });
  }

public:
  const string &address() const noexcept { return _address; }
  const string &name() const noexcept { return name_net; }
  const string &name_short() const noexcept { return _name_short; }
  bool match_name(csv name) const noexcept {
    constexpr string_view delim{"@"};

    std::vector<string_view> parts;
    boost::algorithm::split(parts, name_net, [=](const auto c) { return c == delim.front(); });

    return std::ranges::any_of(parts.begin(), parts.end(),
                               [&name](const auto &part) { return part == name; });
  }

  uint16_t port() const noexcept { return _port; }
  const string &protocol() const noexcept { return _protocol; }
  const string &type() const noexcept { return _type; }
  bool txtKeyExists(csv key) const noexcept { return findTxtByKey(key) != _txt_list.end(); }

  template <typename T = string> const T &txtVal(csv key) const {
    if (auto found = findTxtByKey(key); found != _txt_list.end()) {
      return found->val<T>();
    }

    throw(std::runtime_error("txt key not found"));
  }

  // misc debug
  string inspect() const noexcept;

private:
  // order dependent
  string hostname;
  string name_net;
  string _address;
  string _type;
  uint16_t _port;
  string _protocol;
  zc::TxtList _txt_list;

  // order independent
  string name_mac;
  string _name_short;

  std::any _browser;
  std::any _resolver;

public:
  static constexpr csv module_id{"mdns.zservice"};
};

using ZeroConfMap = std::map<string, ZeroConf>;
using ZeroConfFut = std::shared_future<ZeroConf>;
using ZeroConfProm = std::promise<ZeroConf>;
using ZeroConfPromMap = std::map<string, ZeroConfProm>;

} // namespace pierre