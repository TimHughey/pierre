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

#include <array>
#include <cstdarg>
#include <fmt/format.h>
#include <list>
#include <memory>
#include <plist/plist++.h>
#include <string>
#include <string_view>
#include <vector>

#include "packet/content.hpp"

namespace pierre {
namespace packet {

class Aplist {
public:
  static constexpr bool DEFER_DICT = false;

public:
  enum Embedded : uint8_t { GetInfoRespStage1 = 0 };
  enum Level : uint32_t { Root = 1, Second, Third, Fourth };

  typedef const char *ccs;
  typedef std::string string;
  typedef const std::string &csr;
  typedef const std::string_view csv;

  typedef std::vector<string> ArrayStrings;
  typedef std::vector<Aplist> ArrayDicts;
  typedef std::vector<ccs> Dictionaries;
  typedef std::shared_ptr<uint8_t[]> Binary;

public:
  Aplist(bool allocate = true);
  Aplist(Aplist &&ap); // allow move construction

  Aplist(const Content &content) { fromContent(content); }
  Aplist(const Dictionaries &dicts);
  Aplist(Embedded embedded);

  // create a new Aplist using the dict at the specified path
  Aplist(const Aplist &src, Level level, ...);

  Aplist(const Aplist &ap) = delete; // no copies

  ~Aplist();

  Aplist &operator=(const Aplist &ap) = delete; // no copy assignment
  Aplist &operator=(Aplist &&ap);               // allow move assignment
  Aplist &operator=(const Content &content);    // allow assignment from Content

  Aplist &clear();

  // general API
  // NOTE: api naming convention is dict* to support subclassing

  // sets the base node for all dict* get/set to something other
  // than the root
  Aplist baseNode(Level level, plist_type type, ...);

  Binary toBinary(size_t &bytes) const;

  bool compareString(ccs path, ccs compare);
  bool compareStringViaPath(ccs compare, uint32_t path_count, ...) const;

  void dump(csv prefix) const;
  void dump(plist_t sub_dict = nullptr, csv prefix = csv()) const;
  bool empty() const;
  bool exists(ccs path);
  bool exists(const std::vector<ccs> &items);

  bool getBool(ccs path, bool &dest);
  bool getBool(Level level, ...);
  const string getData(Level level, ...);
  plist_t getItem(ccs path);

  bool getString(ccs path, string &dest);
  std::string getStringConst(Level level, ...);

  bool getStringArray(ccs path, ccs node, ArrayStrings &array_strings);

  // retrive the uint64_t at the named node (at the root)
  uint64_t getUint(ccs root_key) { return getUint(Level::Root, root_key); }

  // retrieve the uint64_t using path specified
  uint64_t getUint(Level level, ...);

  bool ready() const { return _plist != nullptr; }

  const Aplist &self() const { return (const Aplist &)*this; };
  void setArray(ccs root_key, ArrayDicts &dicts);
  bool setArray(ccs sub_dict_key, ccs key, const ArrayStrings &array_strings);
  void setData(ccs key, const fmt::memory_buffer &buf);
  void setBool(ccs key, double val);
  bool setStringVal(ccs sub_dict_key, ccs key, csr str_val);
  bool setUint(ccs key, uint64_t val) { return setUint(nullptr, key, val); }
  bool setUint(ccs sub_dict, ccs key, uint64_t val);

private:
  // plist_t baseNode() { return (_base) ? _base : _plist; }
  bool checkType(plist_t node, plist_type type) const;
  plist_t dict() { return _plist; }

  Aplist &fromContent(const Content &content);

private:
  plist_t _plist = nullptr;
};

} // namespace packet
} // namespace pierre