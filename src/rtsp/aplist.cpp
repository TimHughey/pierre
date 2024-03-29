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

#include "aplist.hpp"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <exception>
#include <iterator>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>

namespace ranges = std::ranges;

namespace pierre {

Aplist::Aplist(const Dictionaries &dictionaries) {
  _plist = plist_new_dict(); // create the root dictionary

  for (const auto &dict : dictionaries) {
    auto node = plist_new_dict();            // create sub dictionary
    plist_dict_set_item(_plist, dict, node); // add to root
  }
}

Aplist::Aplist(csv mem) { plist_from_memory(mem.data(), mem.size(), &_plist); }

Aplist::Aplist(const Aplist &src, const Steps &steps) {
  auto node = src.fetchNode(steps, PLIST_DICT);

  _plist = (node) ? plist_copy(node) : plist_new_dict();
}

Aplist &Aplist::operator=(const uint8v &content) { return from_uint8v(content); }

Aplist &Aplist::operator=(Aplist &&ap) {
  _plist = ap._plist;
  return *this;
}

uint32_t Aplist::arrayItemCount(const Steps &steps) const {
  if (auto node = fetchNode(steps, PLIST_ARRAY); node != nullptr) {
    return plist_array_get_size(node);
  }

  return 0;
}

bool Aplist::boolVal(const Steps &steps) const {
  auto node = fetchNode(steps, PLIST_BOOLEAN);

  if (node) {
    uint8_t val = 0;
    plist_get_bool_val(node, &val);

    return bool(val);
  }

  return false;
}

bool Aplist::compareString(csv key, csv val) const {
  auto found = plist_dict_get_item(_plist, key.data());

  if ((found == nullptr) || (PLIST_STRING != plist_get_node_type(found))) {
    return false;
  }

  uint64_t len = 0;
  auto ptr = plist_get_string_ptr(found, &len);

  return csv(ptr, len) == val;
}

bool Aplist::compareStringViaPath(csv val, uint32_t path_count, ...) const {
  auto rc = false;
  va_list args;

  va_start(args, path_count); // initialize args before passing through
  auto node = plist_access_pathv(_plist, path_count, args);
  va_end(args); // all done with arg, clean up

  if (node && (PLIST_STRING == plist_get_node_type(node))) {
    // we found a node and it's a string, good.

    uint64_t len = 0;
    auto ptr = plist_get_string_ptr(node, &len);

    if (csv(ptr, len) == val) {
      rc = true;
    }
  }

  return rc;
}

const uint8v Aplist::dataArray(const Steps &steps) const {
  uint8v data;

  auto node = fetchNode(steps, PLIST_DATA);

  if (node == nullptr) return data;

  uint64_t len = 0;
  auto ptr = (uint8_t *)plist_get_data_ptr(node, &len);

  if (ptr && len) {
    for (uint64_t i = 0; i < len; i++) {
      data.emplace_back(ptr[i]);
    }
  }

  return data;
}

bool Aplist::empty() const {
  return ((_plist == nullptr) || (plist_dict_get_size(_plist) == 0)) ? true : false;
}

bool Aplist::exists(csv key) { return getItem(key) != nullptr ? true : false; }

bool Aplist::existsAll(const KeyList &key_list) {
  auto rc = true;

  for (const auto &key : key_list) {
    rc &= exists(key);
  }

  return rc;
}

plist_t Aplist::fetchNode(const Steps &steps, plist_type type) const {
  // empty first and only node, copy the entire plist
  if (steps.front().empty() && (steps.size() == 1)) {
    return _plist;
  }

  plist_t node = _plist; // start at the root

  ranges::for_each(steps.cbegin(), steps.cend(), [&](auto step) {
    if ((step[0] >= '0') && (step[0] <= '9')) { // is this an array index?
      uint32_t idx = std::atoi(step.data());
      node = plist_access_path(node, 1, idx);
    } else {
      node = plist_access_path(node, 1, step.data());
    }
  });

  return (node && (type == plist_get_node_type(node))) ? node : nullptr;
}

void Aplist::format_to(uint8v &content) const noexcept {
  char *data = nullptr;
  uint32_t len = 0;

  plist_to_bin(_plist, &data, &len);

  if (len > 0) {
    auto w = std::back_inserter(content);
    std::copy_n(data, len, w);
  } else {
    auto w = std::back_inserter(content);
    fmt::format_to(w, "formatting failed");
  }

  plist_to_bin_free(data);
}

plist_t Aplist::getItem(csv key) const {
  const auto plist = _plist; // need const for function to be const
  return plist_dict_get_item(plist, key.data());
}

void Aplist::setArray(csv key, const ArrayStrings &array_strings) {
  // create and save nodes from the bottom up
  // first create the array since it's the deepest node
  auto array = plist_new_array();

  // populate the array with copies of the strings passed
  for (const auto &item : array_strings) {
    const auto str = item.c_str();

    auto save_str = plist_new_string(str);
    plist_array_append_item(array, save_str);
  }

  plist_dict_set_item(_plist, key.data(), array);
}

void Aplist::setArray(csv key, const Aplist &dict) {
  auto node = fetchNode({key}, PLIST_ARRAY);

  auto array_dict = plist_copy(dict._plist);

  if (node == nullptr) {
    auto array = plist_new_array();

    plist_array_append_item(array, array_dict);
    plist_dict_set_item(_plist, key.data(), array);
  } else {
    plist_array_append_item(node, array_dict);
  }
}

// add array of strings with key node_name to the dict at key path
bool Aplist::setArray(ccs sub_dict_key, ccs key, const ArrayStrings &array_strings) {
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
  auto sub_dict = getItem(sub_dict_key);

  // just for giggles let's confirm the sub_dict is actually a dictionary
  if (sub_dict && (PLIST_DICT == plist_get_node_type(sub_dict))) {
    plist_dict_set_item(sub_dict, key, array);

    return true;
  }

  constexpr auto msg = "unable to add array to missing or non-dict node";
  fmt::print("{}\n", msg);
  throw(std::runtime_error(msg));
}

void Aplist::setData(csv key, const string &d) noexcept {
  plist_dict_set_item(_plist, key.data(), plist_new_data(d.data(), d.size()));
}

void Aplist::setReal(csv key, double val) {
  plist_dict_set_item(_plist, key.data(), plist_new_real(val));
}

// set a string at a sub_dict_key and ket
bool Aplist::setStringVal(ccs sub_dict_key, ccs key, csr str_val) {
  auto sub_dict = _plist;

  if (sub_dict_key) sub_dict = getItem(sub_dict_key); // get the EXISTING sub dictionary

  // just for giggles let's confirm the sub_dict is actually a dictionary
  if (sub_dict && (PLIST_DICT == plist_get_node_type(sub_dict))) {
    auto cstr = str_val.c_str();
    auto val = plist_new_string(cstr);
    plist_dict_set_item(sub_dict, key, val);

    return true;
  }

  constexpr auto msg = "unable to add string to missing or non-dict node";
  fmt::print("{}\n", msg);
  throw(std::runtime_error(msg));
}

void Aplist::setString(csv key, csr str_val) {
  plist_dict_set_item(_plist, key.data(), plist_new_string(str_val.c_str()));
}

// set a string at a sub_dict_key and ket
bool Aplist::setUint(ccs sub_dict_key, ccs key, uint64_t uint_val) {
  auto sub_dict = _plist;

  // get the EXISTING sub dictionary, if requested
  if (sub_dict_key) {
    sub_dict = getItem(sub_dict_key);
  }

  // just for giggles let's confirm the sub_dict is actually a dictionary
  if (sub_dict && (PLIST_DICT == plist_get_node_type(sub_dict))) {
    auto val = plist_new_uint(uint_val);
    plist_dict_set_item(sub_dict, key, val);

    return true;
  }

  constexpr auto msg = "unable to add uint to missing or non-dict node";
  fmt::print("{}\n", msg);
  throw(std::runtime_error(msg));
}

void Aplist::setUints(const UintList &key_uints) {
  for (const auto &kv : key_uints) {
    auto new_val = plist_new_uint(kv.val);
    plist_dict_set_item(_plist, kv.key.data(), new_val);
  }
}

const Aplist::ArrayStrings Aplist::stringArray(const Steps &steps) const {
  auto node = fetchNode(steps, PLIST_ARRAY);

  ArrayStrings array;

  if (node) {
    // get the number of items in the array
    auto items = plist_array_get_size(node);

    for (uint32_t idx = 0; idx < items; idx++) {
      auto item = plist_array_get_item(node, idx);

      if (item && (PLIST_STRING == plist_get_node_type(item))) {
        uint64_t len = 0;
        auto str_ptr = plist_get_string_ptr(item, &len);

        array.emplace_back(string(str_ptr));
      }
    }
  }

  return array;
}

csv Aplist::stringView(const Steps &steps) const {
  auto node = fetchNode(steps, PLIST_STRING);

  if (node) {
    uint64_t len = 0;
    auto ptr = plist_get_string_ptr(node, &len);

    return csv(ptr, len);
  }

  return csv();
}

uint64_t Aplist::uint(const Steps &steps) const {
  uint64_t val = 0;
  auto node = fetchNode(steps, PLIST_UINT);

  if (node) plist_get_uint_val(node, &val);

  return val;
}

bool Aplist::checkType(plist_t node, plist_type type) const {
  return (node && (type == plist_get_node_type(node)));
}

Aplist &Aplist::from_uint8v(const uint8v &content) {
  clear(); // ensure there isn't an existing dict

  static constexpr csv header{"bplist00"};

  if (content.view(0, header.size()) == header) {
    // create the plist and wrap the pointer
    plist_from_memory(content.view().data(), content.size(), &_plist);
  }

  return *this;
}

} // namespace pierre
