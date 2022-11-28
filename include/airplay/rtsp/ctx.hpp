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

#include "base/aes/ctx.hpp"
#include "base/content.hpp"
#include "base/elapsed.hpp"
#include "base/headers.hpp"
#include "base/host.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"

#include <memory>
#include <optional>

namespace pierre {
namespace rtsp {

class Ctx : public std::enable_shared_from_this<Ctx> {

public:
  static auto create() { return std::shared_ptr<Ctx>(new Ctx()); }

  auto ptr() noexcept { return shared_from_this(); }

private:
  Ctx() noexcept : cseq{0}, active_remote{0} {}

public:
  int64_t cseq;
  int64_t active_remote;

public:
  static constexpr csv module_id{"RTSP_CTX"};
};

} // namespace rtsp
} // namespace pierre