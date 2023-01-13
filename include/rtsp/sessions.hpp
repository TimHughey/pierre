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
#include "base/threads.hpp"

#include <map>
#include <memory>
#include <optional>

namespace pierre {

namespace rtsp {

class Sessions {
private:
  struct snode {
    snode(int64_t active_remote, const string &dacp_id, io_context &io_ctx) noexcept
        : active_remote(active_remote),          //
          dacp_id(dacp_id),                      //
          key(make_key(active_remote, dacp_id)), //
          io_ctx(io_ctx)                         //
    {}

    static const string make_key(int64_t active_remote, const string &dacp_id) noexcept;

    // order dependent
    const int64_t active_remote;
    const string dacp_id;
    const string key;
    io_context &io_ctx;

    // order independent
    std::jthread thread;
  };

public:
  Sessions(io::contexts &contexts) noexcept
      : contexts(contexts) // need to save a ref since we start new workers,
  {}

private:
  // order dependent
  io::contexts &contexts;

  // order independent
  std::map<std::string, snode> node_map;

public:
  static constexpr csv module_id{"rtsp.sessions"};
};

} // namespace rtsp
} // namespace pierre
