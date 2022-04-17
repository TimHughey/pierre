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
#include <exception>
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

Aplist::Aplist(const Dictionaries &dictionaries) {
  _plist = plist_new_dict(); // create the root dictionary

  for (const auto &dict : dictionaries) {
    auto node = plist_new_dict();            // create sub dictionary
    plist_dict_set_item(_plist, dict, node); // add to root
  }
}

Aplist::~Aplist() {
  std::for_each(_keeper.begin(), _keeper.end(), [](auto pl) { plist_free(pl); });
}

Aplist::Binary Aplist::dictBinary(size_t &bytes) const {
  char *data = nullptr;
  uint32_t len = 0;

  plist_to_bin(_plist, &data, &len);

  bytes = (size_t)len;

  auto ptr = std::shared_ptr<uint8_t[]>((uint8_t *)data);

  return ptr;
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

void Aplist::dictDump(plist_t sub_dict) const {
  auto dump_dict = (sub_dict) ? sub_dict : _plist;

  char *buf = nullptr;
  uint32_t bytes = 0;

  plist_to_xml(dump_dict, &buf, &bytes);

  fmt::print("\nDICT DUMP dict={} ", fmt::ptr(dump_dict));

  if (buf) {
    fmt::print("buf={} bytes={}\n", fmt::ptr(buf), bytes);
    fmt::print("{}\n", buf);
  } else {
    fmt::print("DUMP FAILED\n");
  }
}

bool Aplist::dictItemExists(ccs path) { return dictGetItem(path) != nullptr ? true : false; }

plist_t Aplist::dictGetItem(ccs path) { return plist_dict_get_item(_plist, path); }

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

bool Aplist::dictGetStringArray(ccs path, ccs node, ArrayStrings &array_strings) {
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

// add am array of strings with key node_name to the dict at key path
bool Aplist::dictSetStringArray(ccs sub_dict_key, ccs key, const ArrayStrings &array_strings) {
  // create and save nodes from the bottom up
  // first create the array since it's the deepest node
  auto array = plist_new_array();

  // populate the array with copies of the strings passed
  for (const auto &item : array_strings) {
    const auto str = item.c_str();

    auto save_str = plist_new_string(str);
    plist_array_append_item(array, save_str);
  }

  // get the EXISTING sub dictionary
  auto sub_dict = dictGetItem(sub_dict_key);

  // just for giggles let's confirm the sub_dict is actually a dictionary
  if (sub_dict && (PLIST_DICT == plist_get_node_type(sub_dict))) {
    plist_dict_set_item(sub_dict, key, array);

    return true;
  }

  constexpr auto msg = "unable to add array to missing or non-dict node";
  fmt::print("{}\n", msg);
  throw(runtime_error(msg));
}

// set a string at a sub_dict_key and ket
bool Aplist::dictSetStringVal(ccs sub_dict_key, ccs key, csr str_val) {
  auto sub_dict = _plist;

  if (sub_dict_key) {
    // get the EXISTING sub dictionary
    sub_dict = dictGetItem(sub_dict_key);
  }

  // just for giggles let's confirm the sub_dict is actually a dictionary
  if (sub_dict && (PLIST_DICT == plist_get_node_type(sub_dict))) {
    auto cstr = str_val.c_str();
    auto val = plist_new_string(cstr);
    plist_dict_set_item(sub_dict, key, val);

    return true;
  }

  constexpr auto msg = "unable to add string to missing or non-dict node";
  fmt::print("{}\n", msg);
  throw(runtime_error(msg));
}

// set a string at a sub_dict_key and ket
bool Aplist::dictSetUint(ccs sub_dict_key, ccs key, uint32_t uint_val) {
  auto sub_dict = _plist;

  // get the EXISTING sub dictionary, if requested
  if (sub_dict_key) {
    sub_dict = dictGetItem(sub_dict_key);
  }

  // just for giggles let's confirm the sub_dict is actually a dictionary
  if (sub_dict && (PLIST_DICT == plist_get_node_type(sub_dict))) {
    auto val = plist_new_uint(uint_val);
    plist_dict_set_item(sub_dict, key, val);

    return true;
  }

  constexpr auto msg = "unable to add uint to missing or non-dict node";
  fmt::print("{}\n", msg);
  throw(runtime_error(msg));
}

void Aplist::track(plist_t item) {
  if (item) {
    _keeper.emplace_back(item);
  }
}

} // namespace rtsp
} // namespace pierre