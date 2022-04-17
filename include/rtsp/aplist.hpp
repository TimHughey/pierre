/*
    Pierre - Custom Light Show for Wiss Landing
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
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include <cstdarg>
#include <plist/plist++.h>
#include <string>
#include <vector>

#include "content.hpp"

namespace pierre {
namespace rtsp {

class Aplist {
public:
  typedef const char *ccs;
  typedef std::string string;
  typedef const std::string &csr;

  typedef std::vector<string> ArrayStrings;
  typedef std::vector<ccs> Dictionaries;

public:
  Aplist();
  Aplist(const Content &content);
  Aplist(const Dictionaries &dicts);
  ~Aplist();

  // general API
  // NOTE: api naming convention is dict* to support subclassing
  void dictDump(plist_t sub_dict = nullptr) const;
  bool dictCompareString(ccs path, ccs compare);
  bool dictItemExists(ccs path);

  bool dictGetBool(ccs path, bool &dest);
  plist_t dictGetItem(ccs path);

  bool dictGetString(ccs path, string &dest);
  bool dictGetStringArray(ccs path, ccs node, ArrayStrings &array_strings);
  bool dictReady() const { return _plist != nullptr; }

  bool dictSetStringArray(ccs sub_dict_key, ccs key,
                          const ArrayStrings &array_strings);
  bool dictSetStringVal(ccs sub_dict_key, ccs key, csr str_val);

private:
  void track(plist_t pl);

private:
  plist_t _plist = nullptr;

  std::vector<plist_t> _keeper;
};

} // namespace rtsp
} // namespace pierre