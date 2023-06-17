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

#include "base/conf/toml.hpp"
#include "base/types.hpp"
#include "desk/msg/data.hpp"
#include "desk/unit/names.hpp"

#include <fmt/format.h>

namespace pierre {
namespace desk {

class Unit {
  friend class Units;
  friend struct fmt::formatter<Unit>;

public:
  Unit(const toml::table &t) noexcept : frame_len{0} {
    using addr_t = decltype(address);

    t["name"].visit([this](const toml::value<string> &v) { name = *v; });
    t["addr"].visit([this](const toml::value<int64_t> &v) { address = static_cast<addr_t>(*v); });
    t["type"].visit([this](const toml::value<string> &v) { type = *v; });

    t["frame_len"].visit([this](const toml::value<int64_t> &v) {
      frame_len = static_cast<decltype(frame_len)>(*v);
    });
  }

  // all units, no matter subclass, must support
  virtual void activate() noexcept {}
  virtual void dark() noexcept {}

  const auto &name_str() const noexcept { return name; }

  // message processing loop
  virtual void prepare() noexcept {}
  virtual void update_msg(DataMsg &msg) noexcept { msg.noop(); }

protected:
  // order dependent
  string name;
  string type;
  uint16_t address;
  size_t frame_len;
};

} // namespace desk
} // namespace pierre

/// @brief Custom formatter for Unit
template <> struct fmt::formatter<pierre::desk::Unit> : formatter<std::string> {
  using Unit = pierre::desk::Unit;

  // parse is inherited from formatter<string>.
  template <typename FormatContext> auto format(const Unit &u, FormatContext &ctx) const {
    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{:<13} {:<9} addr={}", u.name, u.type, u.address);

    if (u.frame_len) fmt::format_to(w, " frame_len={}", u.frame_len);

    return formatter<std::string>::format(msg, ctx);
  }
};
