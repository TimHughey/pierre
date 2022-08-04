/*
Pierre - Custom audio processing for light shows at Wiss Landing
Copyright (C) 2022  Tim Hughey

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "base/typical.hpp"

#include <any>
#include <memory>
#include <ranges>
#include <vector>

namespace pierre {

namespace zc {
class Txt;
typedef std::vector<Txt> TxtList;

class Txt {
public:
  Txt(ccs key_ccs, ccs val_ccs) : key_string(key_ccs) {
    string val(val_ccs);

    if (ranges::all_of(val, [](char c) { return (c >= '0') && (c <= '9'); })) {
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

class ZeroConfService;
typedef std::shared_ptr<ZeroConfService> shZeroConfService;
typedef std::vector<shZeroConfService> ZeroConfServiceList;

class ZeroConfService : public std::enable_shared_from_this<ZeroConfService> {
private:
  struct Details {
    ccs name;
    ccs address;
    ccs type;
    uint16_t port;
    ccs protocol;
    zc::TxtList txt_list;
  };

private: // access via create()
  ZeroConfService(Details d)
      : _name(d.name),         // hostname
        _address(d.address),   // host address
        _type(d.type),         // address family (IPv4 or IPv6)
        _port(d.port),         // service port
        _protocol(d.protocol), // protocol
        _txt_list(d.txt_list)  // txt records
  {}

private:
  const auto findTxtByKey(csv key) const {
    return ranges::find_if(_txt_list, [=](auto &txt) { return txt.key() == key; });
  }

public:
  static shZeroConfService create(Details &&details) {
    return shZeroConfService(new ZeroConfService(std::forward<Details>(details)));
  }

  csr address() const { return _address; }
  csr name() const { return _name; }
  uint16_t port() const { return _port; }
  csr protocol() const { return _protocol; }
  csr type() const { return _type; }
  bool txtKeyExists(csv key) { return findTxtByKey(key) != _txt_list.end(); }

  template <typename T = string> const T &txtVal(csv key) const {
    if (auto found = findTxtByKey(key); found != _txt_list.end()) {
      return found->val<T>();
    }

    throw(std::runtime_error("txt key not found"));
  }

  // misc debug
  string inspect() const {
    string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{} name={} {} {}:{} TXT: ", _type, _name, _protocol, _address, _port);

    ranges::for_each(_txt_list, [w = std::back_inserter(msg)](const zc::Txt &txt) {
      fmt::format_to(w, "{}=", txt.key());

      if (txt.number()) {
        fmt::format_to(w, "{} (number) ", txt.val<uint32_t>());
      } else {
        fmt::format_to(w, "{} ", txt.val());
      }
    });

    return msg;
  }

private:
  string _name;
  string _address;
  string _type;
  uint16_t _port;
  string _protocol;
  zc::TxtList _txt_list;
};

} // namespace pierre