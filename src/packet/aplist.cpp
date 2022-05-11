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
#include <array>
#include <cstdarg>
#include <exception>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <string>

#include "packet/aplist.hpp"

// embedded binary data via ld (see CMakeLists.txt)
extern uint8_t _binary_get_info_resp_plist_start[];
extern uint8_t _binary_get_info_resp_plist_end;
extern uint8_t _binary_get_info_resp_plist_size;
namespace pierre {
namespace packet {

using namespace std;

Aplist::Aplist(bool allocate) {
  if (allocate != DEFER_DICT) {
    _plist = plist_new_dict();
  }
}

Aplist::Aplist(Aplist &&ap) {
  _plist = ap._plist;
  ap._plist = nullptr;
}

Aplist::Aplist(const Dictionaries &dictionaries) {
  _plist = plist_new_dict(); // create the root dictionary

  for (const auto &dict : dictionaries) {
    auto node = plist_new_dict();            // create sub dictionary
    plist_dict_set_item(_plist, dict, node); // add to root
  }
}

Aplist::Aplist(Embedded embedded) {
  const char *begin = nullptr;
  const char *end = nullptr;

  switch (embedded) {
    case GetInfoRespStage1:
      begin = (const char *)&_binary_get_info_resp_plist_start;
      end = (const char *)&_binary_get_info_resp_plist_end;
      break;
  }

  if (begin) {
    size_t bytes = end - begin;
    plist_from_memory(begin, bytes, &_plist);
  }
}

Aplist::Aplist(const Aplist &src, Level level, ...) {
  va_list args;

  va_start(args, level);
  auto node = plist_access_pathv(src._plist, level, args);
  va_end(args);

  if (node && (PLIST_DICT == plist_get_node_type(node))) {
    _plist = plist_copy(node);
  } else {
    _plist = plist_new_dict();
  }
}

Aplist::~Aplist() {
  if (_plist) {
    plist_free(_plist);
    _plist = nullptr;
  }
}

Aplist &Aplist::operator=(const Content &content) { return fromContent(content); }

Aplist &Aplist::clear() {
  if (_plist) {
    plist_free(_plist);
    _plist = nullptr;
  }

  return *this;
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

  uint64_t len = 0;
  ccs val = plist_get_string_ptr(item, &len);

  if (len && (strncmp(val, compare, len) == 0)) {
    rc = true;
  }

  return rc;
}

bool Aplist::dictCompareStringViaPath(ccs compare, uint32_t path_count, ...) const {
  auto rc = false;
  va_list args;

  va_start(args, path_count); // initialize args before passing through
  auto node = plist_access_pathv(_plist, path_count, args);
  va_end(args); // all done with arg, clean up

  if (node && (PLIST_STRING == plist_get_node_type(node))) {
    // we found a node and it's a string, good.

    uint64_t len = 0;
    ccs val = plist_get_string_ptr(node, &len);

    if (len && (strncmp(val, compare, len) == 0)) {
      rc = true;
    }
  }

  return rc;
}

void Aplist::dictDump(csv prefix) const { dictDump(nullptr, prefix); }

void Aplist::dictDump(plist_t sub_dict, csv prefix) const {
  auto dump_dict = (sub_dict) ? sub_dict : _plist;

  if (prefix.size()) {
    fmt::print("{}\n", prefix);
  } else {
    fmt::print("\n");
  }

  if (dump_dict == nullptr) {
    fmt::print("DICT DUMP dict={} is empty\n", fmt::ptr(dump_dict));
    return;
  }

  fmt::print("DICT DUMP dict={} ", fmt::ptr(dump_dict));

  char *buf = nullptr;
  uint32_t bytes = 0;

  plist_to_xml(dump_dict, &buf, &bytes);

  if (bytes > 0) {
    fmt::print("buf={} bytes={}\n", fmt::ptr(buf), bytes);
    fmt::print("{}\n", buf);
  } else {
    fmt::print("DUMP FAILED\n");
  }
}

bool Aplist::dictEmpty() const { return _plist == nullptr; }

bool Aplist::dictItemExists(ccs path) { return dictGetItem(path) != nullptr ? true : false; }

bool Aplist::dictItemsExist(const std::vector<ccs> &items) {
  auto rc = true;

  for (const auto &item : items) {
    rc &= dictItemExists(item);
  }

  return rc;
}

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

bool Aplist::dictGetBool(Level level, ...) {
  va_list args;
  auto bool_val = false;

  va_start(args, level);
  auto node = plist_access_pathv(_plist, level, args);
  va_end(args);

  if (checkType(node, PLIST_BOOLEAN)) {
    uint8_t pseudo_bool = 255;
    plist_get_bool_val(node, &pseudo_bool);

    // pseudo bool unchanged, failed
    bool_val = (pseudo_bool == 255) ? false : true;
  }

  return bool_val;
}

const std::string Aplist::dictGetData(Level level, ...) {
  va_list args;

  va_start(args, level); // initialize args before passing through
  auto node = plist_access_pathv(_plist, level, args);
  va_end(args); // all done with arg, clean up

  if (checkType(node, PLIST_DATA)) {
    uint64_t len;
    ccs ptr = plist_get_data_ptr(node, &len);

    return std::string(csv(ptr, len));
  }

  return std::string();
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

std::string Aplist::dictGetStringConst(Level level, ...) {
  va_list args;

  va_start(args, level); // initialize args before passing through
  auto node = plist_access_pathv(_plist, level, args);
  va_end(args); // all done with arg, clean up

  if (checkType(node, PLIST_STRING)) {
    uint64_t len;
    ccs ptr = plist_get_string_ptr(node, &len);

    return std::string(csv(ptr, len));
  }

  return std::string("not found");
}

bool Aplist::dictGetStringArray(ccs level1_key, ccs key, ArrayStrings &array_strings) {
  auto rc = false;

  // start at the root
  auto node = _plist;

  // do the level1_key and key point to an array?
  if (checkType(node, PLIST_DICT)) {
    // ok, the root is a dict so let's look for the level1_key
    auto level1_dict = plist_dict_get_item(node, level1_key);

    if (checkType(level1_dict, PLIST_DICT)) {
      // fine, the level one key is also a dict, now look for key
      auto key_dict = plist_dict_get_item(level1_dict, key);

      if (checkType(key_dict, PLIST_ARRAY)) {
        // great, key is an array, set node to key_dict
        node = key_dict;
        // yes, there will be a second check of the type below, so what
      }
    }
  }

  if (checkType(node, PLIST_ARRAY)) {
    // good we have an array

    // get the number of items in the array
    auto items = plist_array_get_size(node);

    for (uint32_t idx = 0; idx < items; idx++) {
      auto array_item = plist_array_get_item(node, idx);

      if (array_item && (PLIST_STRING == plist_get_node_type(array_item))) {
        uint64_t len = 0;
        auto str_ptr = plist_get_string_ptr(array_item, &len);

        array_strings.emplace_back(string(str_ptr));
      }
      // we found an array of strings and have copied them
      rc = true;
    }
  }

  return rc;
}

uint64_t Aplist::dictGetUint(Level level, ...) {
  va_list args;

  va_start(args, level); // initialize args before passing through
  auto node = plist_access_pathv(_plist, level, args);
  va_end(args); // all done with arg, clean up

  if (checkType(node, PLIST_UINT)) {
    uint64_t val = 0;
    plist_get_uint_val(node, &val);

    return val;
  }

  throw(std::out_of_range("unknown dict key"));
}

void Aplist::dictSetArray(ccs root_key, ArrayDicts &dicts) {
  auto array = plist_new_array();

  // add each dict to the newly created array
  for_each(dicts.begin(), dicts.end(), [array](auto &item) {
    auto append = plist_copy(item.dict());  // need our own copy to avoid double mem dealloc
    plist_array_append_item(array, append); // append the sub dict
  });

  // place the array at the specified key
  plist_dict_set_item(_plist, root_key, array);
}

void Aplist::dictSetData(ccs key, const fmt::memory_buffer &buf) {
  auto data = plist_new_data(buf.data(), buf.size());
  plist_dict_set_item(_plist, key, data);
}

void Aplist::dictSetReal(ccs key, double val) {
  auto data = plist_new_real(val);
  plist_dict_set_item(_plist, key, data);
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
bool Aplist::dictSetUint(ccs sub_dict_key, ccs key, uint64_t uint_val) {
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

bool Aplist::checkType(plist_t node, plist_type type) const {
  return (node && (type == plist_get_node_type(node)));
}

Aplist &Aplist::fromContent(const Content &content) {
  clear(); // ensure there isn't an existing dict

  constexpr auto header = "bplist00";
  constexpr auto header_len = strlen(header);

  if (content.size() > header_len) {
    const char *data = (const char *)content.data();

    // create the plist and wrap the pointer
    plist_from_memory(data, content.size(), &_plist);
  }

  return *this;
}

} // namespace packet
} // namespace pierre