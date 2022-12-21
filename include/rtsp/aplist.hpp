//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "rtsp/content.hpp"

#include <array>
#include <cstdarg>
#include <exception>
#include <fmt/format.h>
#include <list>
#include <memory>
#include <plist/plist++.h>
#include <vector>

namespace pierre {

class Aplist {
public:
  struct KeyUint {
    csv key;
    uint64_t val;
  };

  using ArrayStrings = std::vector<string>;
  using ArrayDicts = std::vector<Aplist>;
  using ConstArrayDicts = std::vector<const Aplist &>;
  using Dictionaries = std::vector<ccs>;
  using KeyList = std::vector<string_view>;
  using Steps = std::vector<string_view>;
  using UintList = std::vector<KeyUint>;

public:
  static constexpr csv ROOT{""};

public:
  /// @brief Default constructor (dict not allocated)
  Aplist() noexcept : _plist(plist_new_dict()) {}
  ~Aplist() {
    if (_plist) plist_free(_plist);
  }

  /// @brief Move constructor
  /// @param ap Other Aplist to move from
  Aplist(Aplist &&ap) noexcept : _plist(std::exchange(ap._plist, nullptr)) {}

  /// @brief Create Aplist from a vector of chars
  /// @param xml Vector of char data
  Aplist(const std::vector<char> &xml) noexcept { plist_from_xml(xml.data(), xml.size(), &_plist); }

  Aplist(const Content &content) noexcept { fromContent(content); }
  Aplist(const Dictionaries &dicts);
  Aplist(csv mem);
  Aplist(const Aplist &src, const Steps &steps); //

  Aplist &operator=(const Aplist &ap) = delete; // no copy assignment
  Aplist &operator=(Aplist &&ap);               // allow move assignment
  Aplist &operator=(const Content &content);    // allow assignment from Content

  uint32_t arrayItemCount(const Steps &steps) const;

  Aplist &clear() noexcept {
    if (_plist) plist_free(_plist);
    _plist = plist_new_dict();

    return *this;
  }

  // general API
  bool boolVal(const Steps &steps) const;

  bool compareString(csv key, csv val) const;
  bool compareStringViaPath(csv val, uint32_t path_count, ...) const;

  const uint8v dataArray(const Steps &steps) const;

  bool empty() const;
  bool exists(csv key);
  bool existsAll(const KeyList &key_list);

  plist_t fetchNode(const Steps &steps, plist_type type = PLIST_DICT) const;

  void format_to(Content &content) const noexcept;

  void format_to(auto &w) const noexcept {
    char *xml{nullptr};
    uint32_t bytes{0};

    plist_to_xml(_plist, &xml, &bytes);

    if (bytes) {
      fmt::format_to(w, "{}", xml);
    } else {
      fmt::format_to(w, "<<failed to format plist as xml>>");
    }

    plist_to_xml_free(xml);
  }

  bool ready() const { return _plist != nullptr; }

  const Aplist &self() const { return (const Aplist &)*this; };

  void setArray(csv key, const ArrayStrings &array);
  void setArray(csv key, const Aplist &dict);
  bool setArray(ccs sub_dict_key, ccs key, const ArrayStrings &array_strings);

  void setData(csv key, const string &d) noexcept;

  void setReal(csv key, double val);
  void setString(csv key, csr str_val);

  // NEW!
  bool setStringVal(ccs sub_dict_key, std::pair<string, string> key_val) noexcept {
    return setStringVal(sub_dict_key, key_val.first.data(), key_val.second);
  }

  bool setStringVal(ccs sub_dict_key, ccs key, csr str_val);
  bool setUint(csv key, uint64_t val) { return setUint(nullptr, key.data(), val); }
  bool setUint(ccs sub_dict, ccs key, uint64_t val);

  bool setUint(std::pair<csv, uint64_t> key_val) {
    return setUint(nullptr, key_val.first.data(), key_val.second);
  }

  void setUints(const UintList &uints);

  const ArrayStrings stringArray(const Steps &steps) const;
  csv stringView(const Steps &steps) const;

  uint64_t uint(const Steps &steps) const;

  // misc debug
  void dump(csv prefix) const;
  void dump(plist_t sub_dict = nullptr, csv prefix = csv()) const;
  const string inspect(plist_t what_dict = nullptr) const;

private:
  // plist_t baseNode() { return (_base) ? _base : _plist; }
  bool checkType(plist_t node, plist_type type) const;
  plist_t dict() { return _plist; }

  Aplist &fromContent(const Content &content);
  plist_t getItem(csv key) const;

private:
  plist_t _plist{nullptr};

public:
  static constexpr csv module_id{"APLIST"};
};

} // namespace pierre