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

#include "base/conf/token.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"

#include <array>
#include <concepts>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <memory>

namespace pierre {
namespace conf {

class watch : public token {
  friend struct fmt::formatter<watch>;

public:
  using f_time = std::filesystem::file_time_type;
  using fs_path = std::filesystem::path;
  using stc_tp = std::chrono::steady_clock::time_point;

public:
  /// @brief Create config token with populated subtable that is not registered
  ///        for change notifications
  /// @param mid module_id (aka root)
  watch(csv mid, Millis stable = 1s) noexcept
      : token(mid), stable_ms{stable}, in_fd{0}, w_fd{0}, last_write_at{0ns} {}

  virtual ~watch() noexcept;

  bool changed() noexcept;

  bool initiate() noexcept;

private:
  // order independent
  const Millis stable_ms;
  int in_fd;            // inotify fd
  int w_fd;             // specific watch fd
  f_time last_write_at; // last file write time (for stable check)

  // order independent
  fs_path file_path;

public:
  static constexpr auto module_id{"conf.watch"sv};
};

} // namespace conf
} // namespace pierre

template <> struct fmt::formatter<pierre::conf::watch> : formatter<std::string> {
  using ctoken = pierre::conf::token;

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::conf::watch &tok, FormatContext &ctx) const {

    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "{} ", static_cast<const ctoken &>(tok));
    fmt::format_to(w, "in_fd={} w_fd={}", tok.in_fd, tok.w_fd);

    return formatter<std::string>::format(std::move(msg), ctx);
  }
};