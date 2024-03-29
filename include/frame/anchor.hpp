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

#include "base/pet.hpp"
#include "base/types.hpp"
#include "frame/anchor_data.hpp"
#include "frame/anchor_last.hpp"
#include "frame/clock_info.hpp"

#include <array>
#include <memory>
#include <optional>

namespace pierre {

class Anchor {
public:
  Anchor() = default;
  static void init(); // create shared Anchor

  static AnchorLast get_data(const ClockInfo &clock) noexcept;
  static void save(AnchorData ad) noexcept;
  static void reset() noexcept;

private:
  AnchorLast get_data_impl(const ClockInfo &clock) noexcept;

private:
  std::optional<AnchorData> source;
  AnchorLast last;

public:
  static constexpr auto module_id{"anchor"};

public:
  Anchor(const Anchor &) = delete;            // no copy
  Anchor(Anchor &&) = delete;                 // no move
  Anchor &operator=(const Anchor &) = delete; // no copy assignment
  Anchor &operator=(Anchor &&) = delete;      // no move assignment
};

} // namespace pierre
