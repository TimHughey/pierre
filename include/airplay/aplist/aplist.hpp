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

#include "base/content.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"

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

  typedef std::vector<string> ArrayStrings;
  typedef std::vector<Aplist> ArrayDicts;
  typedef std::shared_ptr<uint8_t[]> Binary;
  typedef std::vector<const Aplist &> ConstArrayDicts;
  typedef std::vector<ccs> Dictionaries;
  typedef std::vector<string_view> KeyList;
  typedef std::vector<string_view> Steps;
  typedef std::vector<KeyUint> UintList;

public:
  static constexpr bool DEFER_DICT = false;
  static constexpr csv ROOT{""};

public:
  Aplist(bool allocate = true);
  Aplist(Aplist &&ap); // allow move construction
  Aplist(const Content &content) { fromContent(content); }
  Aplist(const Dictionaries &dicts);
  Aplist(csv mem);
  Aplist(const Aplist &src, const Steps &steps); //
  Aplist(const Aplist &ap) = delete;             // no copies

  ~Aplist();

  Aplist &operator=(const Aplist &ap) = delete; // no copy assignment
  Aplist &operator=(Aplist &&ap);               // allow move assignment
  Aplist &operator=(const Content &content);    // allow assignment from Content

  uint32_t arrayItemCount(const Steps &steps) const;

  Aplist &clear();

  // general API

  bool boolVal(const Steps &steps) const;
  Binary toBinary(size_t &bytes) const;

  bool compareString(csv key, csv val) const;
  bool compareStringViaPath(csv val, uint32_t path_count, ...) const;

  const uint8v dataArray(const Steps &steps) const;

  bool empty() const;
  bool exists(csv key);
  bool existsAll(const KeyList &key_list);

  plist_t fetchNode(const Steps &steps, plist_type type = PLIST_DICT) const;

  bool ready() const { return _plist != nullptr; }

  const Aplist &self() const { return (const Aplist &)*this; };

  void setArray(csv key, const ArrayStrings &array);
  void setArray(csv key, const Aplist &dict);
  bool setArray(ccs sub_dict_key, ccs key, const ArrayStrings &array_strings);

  void setData(ccs key, const fmt::memory_buffer &buf);

  void setReal(csv key, double val);
  void setString(csv key, csr str_val);
  bool setStringVal(ccs sub_dict_key, ccs key, csr str_val);
  bool setUint(csv key, uint64_t val) { return setUint(nullptr, key.data(), val); }
  bool setUint(ccs sub_dict, ccs key, uint64_t val);
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
  plist_t _plist = nullptr;

  static constexpr csv moduleId = "APLIST";
};

} // namespace pierre