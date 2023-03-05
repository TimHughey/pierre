// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#pragma once

#include "base/types.hpp"
#include "desk/msg/kv.hpp"
#include "frame/frame.hpp"

#include <variant>
#include <vector>

namespace pierre {
namespace desk {

class kv_store {
private:
  // note: these are the only types that Units set
  using val_t = std::variant<uint16_t, uint32_t, float, bool, int64_t, string>;

  struct key_val_entry {
    string key;
    val_t val;
  };

public:
  kv_store() = default;
  ~kv_store() noexcept {} // delete default copy construct / assign

  kv_store(kv_store &&) = default;
  kv_store &operator=(kv_store &&) = default;

  void add(auto key, auto &&val) noexcept { key_vals.emplace_back(key_val_entry{key, val}); }

  void populate_doc(auto &doc) noexcept {

    for (const auto &entry : key_vals) {
      // visit each entry value and add them to the document at the entry key
      std::visit([&](auto &&_val) { doc[entry.key] = _val; }, entry.val);
    }
  }

private:
  std::vector<key_val_entry> key_vals;

public:
  static constexpr csv module_id{"desk.msg.kv_store"};
};

} // namespace desk
} // namespace pierre
