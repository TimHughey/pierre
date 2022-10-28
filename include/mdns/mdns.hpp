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

#include "base/io.hpp"
#include "base/types.hpp"
#include "mdns/zservice.hpp"

#include <future>
#include <list>
#include <map>
#include <memory>

namespace pierre {

using ZeroConfMap = std::map<string, ZeroConf>;
using ZeroConfFut = std::future<ZeroConf>;
using ZeroConfProm = std::promise<ZeroConf>;
using ZeroConfPromMap = std::map<string, ZeroConfProm>;

using ZeroConfServiceList = std::list<ZeroConf>;

class mDNS {

public:
  mDNS(io_context &io_ctx) noexcept : io_ctx(io_ctx) {}

public:
  static void init(io_context &io_ctx) noexcept;

  static void reset();

public:
  static void browse(csv stype);
  static auto port() noexcept { return PORT; }
  bool start();
  static void update() noexcept;
  static std::future<ZeroConf> zservice(csv name);

private:
  void browse_impl(csv stype);
  void init_self();
  void update_impl();

public:
  // order dependent
  io_context &io_ctx;

  // order independent
  string _dacp_id;

  bool found = false;
  string type;
  string domain;
  string host_name;
  string error;

private:
  string _service_base;
  static constexpr auto PORT{7000};

public:
  static constexpr csv module_id{"mDNS"};
};

} // namespace pierre