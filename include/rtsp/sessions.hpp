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

#include "base/types.hpp"
#include "io/io.hpp"

#include <memory>
#include <shared_mutex>
#include <vector>

namespace pierre {
class Desk;
class MasterClock;

namespace rtsp {

// forward decl in lieu of #include
class Ctx;

class Sessions {

public:
  Sessions() = default;

  void add(std::shared_ptr<Ctx> ctx) noexcept;
  void erase(const std::shared_ptr<Ctx> ctx) noexcept;

  void close_all() noexcept;

  void live(Ctx *ctx) noexcept;

private:
  std::vector<std::shared_ptr<Ctx>> ctxs;
  std::shared_mutex mtx;

public:
  static constexpr csv module_id{"rtsp.sessions"};
};

} // namespace rtsp
} // namespace pierre