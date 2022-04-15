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

#include <algorithm>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <string>

#include "rtsp/aplist.hpp"

namespace pierre {
namespace rtsp {

using namespace std;

Aplist::Aplist() { _plist = plist_new_dict(); }

Aplist::Aplist(const Content &content) {
  const char *data = (const char *)content.data();

  // create the plist and wrap the pointer
  plist_from_memory(data, content.size(), &_plist);
}

Aplist::~Aplist() {
  std::for_each(_keeper.begin(), _keeper.end(),
                [](auto pl) { plist_free(pl); });
}

bool Aplist::dictCompareString(ccs path, ccs compare) {
  auto rc = false;

  auto item = plist_dict_get_item(_plist, path);

  if (item == nullptr)
    return rc;

  track(item);

  uint64_t len = 0;
  ccs val = plist_get_string_ptr(item, &len);

  if (len && (strncmp(val, compare, len) == 0)) {
    rc = true;
  }

  return rc;
}

bool Aplist::dictItemExists(ccs path) {
  return dictGetItem(path) != nullptr ? true : false;
}

plist_t Aplist::dictGetItem(ccs path) {
  auto item = plist_dict_get_item(_plist, path);
  track(item);

  return item;
}

bool Aplist::dictGetBool(ccs path, bool &dest) {
  auto rc = false;
  auto node = plist_dict_get_item(_plist, path);

  dest = false;

  if (node && (PLIST_BOOLEAN == plist_get_node_type(node))) {
    uint8_t pseudo_bool = 255;
    plist_get_bool_val(node, &pseudo_bool);

    // pseudo bool unchanged, failed
    rc = (pseudo_bool == 255) ? false : true;

    if (rc) {
      dest = (pseudo_bool == 0) ? false : true;
    }
  }

  return rc;
}

bool Aplist::dictGetString(ccs path, string &dest) {
  auto rc = false;
  auto node = plist_dict_get_item(_plist, path);

  dest.clear();

  if (node && (PLIST_STRING == plist_get_node_type(node))) {
    uint64_t len = 0;
    dest = plist_get_string_ptr(node, &len);

    rc = true;
  }

  return rc;
}

bool Aplist::dictGetStringArray(ccs path, ccs node,
                                ArrayStrings &array_strings) {
  auto rc = false;

  // get the base path
  auto dict = plist_dict_get_item(_plist, path);

  array_strings.clear();

  if (dict && (PLIST_DICT == plist_get_node_type(dict))) {
    auto dict_node = plist_dict_get_item(dict, node);

    // confirm this is an array
    if (dict_node && (PLIST_ARRAY == plist_get_node_type(dict_node))) {
      // good we found the base dict and the node within it that's an array

      // get the number of items in the array
      auto items = plist_array_get_size(dict_node);

      for (uint32_t idx = 0; idx < items; idx++) {
        auto array_item = plist_array_get_item(dict_node, idx);

        if (array_item && (PLIST_STRING == plist_get_node_type(array_item))) {
          uint64_t len = 0;
          auto str_ptr = plist_get_string_ptr(array_item, &len);

          array_strings.emplace_back(string(str_ptr));
        }
      }
    }

    rc = true;
  }

  return rc;
}

void Aplist::track(plist_t item) {
  if (item) {
    _keeper.emplace_back(item);
  }
}

} // namespace rtsp
} // namespace pierre