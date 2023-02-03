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

void Sessions::close(const std::shared_ptr<Ctx> ctx) noexcept {
  ctx->teardown();
  ctx->close();
}

void Sessions::close_all() noexcept {
  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  std::erase_if(ctxs, [](const auto ctx) {
    ctx->teardown();
    ctx->close();

    return true;
  });
}

std::shared_ptr<Ctx> Sessions::create(io_context &io_ctx, std::shared_ptr<tcp_socket> sock,
                                      Desk *desk) noexcept {
  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  return ctxs.emplace_back(new Ctx(io_ctx, sock, this, desk));
}

void Sessions::live(std::shared_ptr<Ctx> live_ctx) noexcept {
  static constexpr csv fn_id{"live"};

  INFO(module_id, fn_id, "dacp_id={} active_remote={}\n", live_ctx->dacp_id,
       live_ctx->active_remote);

  std::unique_lock lck(mtx, std::defer_lock);
  lck.lock();

  std::erase_if(ctxs, [&](const auto ctx) {
    auto rc = false;

    if (live_ctx != ctx) {
      INFO(module_id, fn_id, "freeing dacp_id={} active_remote={}\n", ctx->dacp_id,
           ctx->active_remote);
      close(ctx);
      rc = true;
    }

    return rc;
  });
}

} // namespace rtsp
} // namespace pierre