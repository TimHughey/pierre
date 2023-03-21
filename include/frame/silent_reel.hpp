
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

#include "frame/reel.hpp"

namespace pierre {

/// @brief Container of Frames
class SilentReel : public Reel {

private:
  static auto constexpr MAX_FRAMES{static_cast<ssize_t>(InputInfo::fps / (1.0 / 15.0))}; // ~330ms

public:
  // construct a reel of audio frames
  SilentReel(ssize_t max_frames = MAX_FRAMES) noexcept;
  ~SilentReel() = default;

  // always flush an entire SilentReel
  bool flush(FlushInfo &) noexcept override { return true; }

  bool silence() const noexcept override { return true; }

public:
  static constexpr csv module_id{"desk.silent_reel"};
};

} // namespace pierre

/// @brief Custom formatter for SilentReel
template <> struct fmt::formatter<pierre::SilentReel> : formatter<std::string> {

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::SilentReel &reel, FormatContext &ctx) const {
    const auto &base = static_cast<pierre::Reel>(reel);

    return formatter<std::string>::format(fmt::format("{}", fmt::format("{} SILENT")), ctx);
  }
};