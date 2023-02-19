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

#include "sessions.hpp"
#include "ctx.hpp"
#include "lcs/logger.hpp"

namespace pierre {
namespace rtsp {

void Sessions::add(std::shared_ptr<Ctx> ctx) noexcept {
  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  ctxs.emplace_back(ctx);
}

void Sessions::close_all() noexcept {
  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  std::erase_if(ctxs, [](auto &ctx) {
    ctx->force_close();
    ctx.reset();

    return true;
  });
}

void Sessions::erase(const std::shared_ptr<Ctx> ctx) noexcept {
  static constexpr csv fn_id{"erase"};
  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  std::erase_if(ctxs, [&ctx](const auto a_ctx) {
    static constexpr csv fn_id{"erase"};

    auto rc = a_ctx == ctx;

    if (rc) INFO_AUTO("{}\n", ctx);

    return rc;
  });

  INFO_AUTO("remaining={}\n", std::ssize(ctxs));
}

void Sessions::live(Ctx *live_ctx) noexcept {
  static constexpr csv fn_id{"live"};

  INFO_AUTO("{}\n", *live_ctx);

  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  std::erase_if(ctxs, [&](const auto ctx) mutable {
    auto rc = false;

    if (live_ctx != ctx.get()) {
      static constexpr csv fn_id{"freeing"};
      INFO_AUTO("{}}\n", ctx);

      ctx->force_close();
      rc = true;
    }

    return rc;
  });
}

} // namespace rtsp
} // namespace pierre