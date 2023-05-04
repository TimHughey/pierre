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

#include <array>
#include <concepts>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <map>
#include <memory>
#include <uuid/uuid.h>

namespace pierre {

struct UUID {
  friend struct fmt::formatter<UUID>;

  UUID() noexcept {
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, storage.data());
  }

  const string operator()() const noexcept { return storage.data(); }

  operator string() const noexcept { return storage.data(); }

private:
  std::array<char, 37> storage{0};
};

} // namespace pierre

template <> struct fmt::formatter<pierre::UUID> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const pierre::UUID &uuid, FormatContext &ctx) const {
    return formatter<std::string>::format(uuid.storage.data(), ctx);
  }
};