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

#include "lcs/config/watch.hpp"
#include "base/pet.hpp"
#include "lcs/config.hpp"
#include "lcs/config/toml.hpp"
#include "lcs/logger.hpp"

#include <fmt/chrono.h>
#include <fmt/os.h>
#include <fmt/std.h>

namespace pierre {
namespace config2 {

using file_clock = std::chrono::file_clock;

watch::watch(asio::io_context &io_ctx, Config *cfg_ptr) noexcept
    : local_strand(asio::make_strand(io_ctx)),
      file_timer(local_strand, steady_clock::now()), cfg_ptr{cfg_ptr},
      file_last_time{cfg_ptr->last_write_time()} //
{
  monitor_file();
}

void watch::check_file() noexcept {
  static constexpr csv fn_id{"check_file"};

  auto now_last_changed = cfg_ptr->last_write_time();

  if (auto diff = now_last_changed - file_last_time; diff > Nanos::zero()) {
    INFO_AUTO(                              //
        "{0} {1:%F}T{1:%R} {2}\n",          //
        cfg_ptr->file_path(),               //
        file_clock::to_sys(file_last_time), //
        pet::humanize(diff));

    file_last_time = now_last_changed;

    if (cfg_ptr->parse()) {
      std::scoped_lock lck(mtx);

      std::for_each(tokens.begin(), tokens.end(), [this](const auto &tok_ref) mutable {
        auto &tok = tok_ref.get();

        const toml::path sub_path{tok.mod_id};
        std::any next_table;

        cfg_ptr->copy(tok.mod_id, next_table);

        tok.notify_of_change(std::move(next_table));
      });
    } else {
      INFO_AUTO("parse failed, {}\n", cfg_ptr->parse_msg);
    }
  }

  monitor_file();
}

void watch::monitor_file() noexcept {

  file_timer.expires_from_now(1s);
  file_timer.async_wait([this](const error_code ec) {
    if (ec) return; // error or shutdown, bail...

    check_file();
  });
}

bool watch::register_token(token &tok, token::lambda &&handler) noexcept {
  static constexpr csv fn_id{"register"};

  INFO_AUTO("mod_id={}\n", tok.mod_id);

  cfg_ptr->copy(tok.mod_id, tok.table);

  tok.registered(std::forward<token::lambda>(handler), this);

  std::scoped_lock lck(mtx);
  tokens.emplace_back(std::ref(tok));

  return true;
}

void watch::unregister_token(token &tok) noexcept {
  static constexpr csv fn_id{"unregister"};

  std::scoped_lock lck(mtx);

  std::erase_if(tokens, [&](const auto &t) {
    const auto &mod_id = t.get().mod_id;
    INFO_AUTO("mod_id={}\n", mod_id);

    return tok.mod_id == mod_id;
  });
}

} // namespace config2
} // namespace pierre