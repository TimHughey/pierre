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

#include "base/io.hpp"
#include "base/logger.hpp"
#include "server/base.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>

namespace pierre {
namespace airplay {

class Servers : public std::enable_shared_from_this<Servers> {

public:
  static std::shared_ptr<Servers> init(io_context &io_ctx) noexcept;
  static auto ptr() noexcept { return self()->shared_from_this(); }
  static void reset() noexcept { self().reset(); }

  Port localPort(ServerType type);
  static void teardown() noexcept {
    static constexpr std::array types{ServerType::Event, ServerType::Control, ServerType::Audio};

    auto &map = ptr()->map;

    for (const auto type : types) {
      auto it = map.find(type);

      if (it != map.end()) {
        auto srv = it->second; // get our own shared_ptr to the server

        srv->teardown(); // ask it to shutdown
        map.erase(it);   // erase it from the map

        INFO(module_id, "TEARDOWN", "server={}\n", fmt::ptr(srv.get()));

        // our shared_ptr to the server falls out of scope
      }
    }
  }

private:
  Servers(io_context &io_ctx) : io_ctx(io_ctx) {}

  std::shared_ptr<server::Base> fetch(ServerType type) noexcept {
    if (map.contains(type) == false) {
      return std::shared_ptr<server::Base>(nullptr);
    }

    return map.at(type);
  }

  static std::shared_ptr<Servers> &self() noexcept;

private:
  // order dependent based on constructor
  io_context &io_ctx;

  // order independent
  std::map<ServerType, std::shared_ptr<server::Base>> map;

public:
  static constexpr csv module_id{"AP_SERVERS"};
};

} // namespace airplay
} // namespace pierre
