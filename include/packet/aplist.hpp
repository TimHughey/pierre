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
  Aplist();
  Aplist(const Content &content);
  Aplist(const Dictionaries &dicts);
  Aplist(Embedded embedded);

  // create a new Aplist using the dict at the specified path
  Aplist(const Aplist &src, Level level, ...);

  ~Aplist();

  // general API
  // NOTE: api naming convention is dict* to support subclassing

  // sets the base node for all dict* get/set to something other
  // than the root
  Aplist dictBaseNode(Level level, plist_type type, ...);

  Binary dictBinary(size_t &bytes) const;

  bool dictCompareString(ccs path, ccs compare);
  bool dictCompareStringViaPath(ccs compare, uint32_t path_count, ...) const;

  void dictDump(csv prefix) const;
  void dictDump(plist_t sub_dict = nullptr, csv prefix = csv()) const;
  bool dictEmpty() const;
  bool dictItemExists(ccs path);
  bool dictItemsExist(const std::vector<ccs> &items);

  bool dictGetBool(ccs path, bool &dest);
  bool dictGetBool(Level level, ...);
  const string dictGetData(Level level, ...);
  plist_t dictGetItem(ccs path);

  bool dictGetString(ccs path, string &dest);
  std::string dictGetStringConst(Level level, ...);

  bool dictGetStringArray(ccs path, ccs node, ArrayStrings &array_strings);

  // retrive the uint64_t at the named node (at the root)
  uint64_t dictGetUint(ccs root_key) { return dictGetUint(Level::Root, root_key); }

  // retrieve the uint64_t using path specified
  uint64_t dictGetUint(Level level, ...);

  bool dictReady() const { return _plist != nullptr; }

  const Aplist &dictSelf() const { return (const Aplist &)*this; };
  void dictSetArray(ccs root_key, ArrayDicts &dicts);
  void dictSetData(ccs key, const fmt::memory_buffer &buf);

  bool dictSetStringArray(ccs sub_dict_key, ccs key, const ArrayStrings &array_strings);
  bool dictSetStringVal(ccs sub_dict_key, ccs key, csr str_val);
  bool dictSetUint(ccs sub_dict, ccs key, uint64_t val);

private:
  plist_t baseNode() { return (_base) ? _base : _plist; }
  bool checkType(plist_t node, plist_type type) const;
  plist_t dict() { return _plist; }
  void track(plist_t pl);

private:
  plist_t _plist = nullptr;
  plist_t _base = nullptr;

  std::vector<plist_t> _keeper;
};

} // namespace packet
} // namespace pierre