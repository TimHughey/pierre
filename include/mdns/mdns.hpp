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
#include "mdns/service.hpp"
#include "mdns/zservice.hpp"

#include <atomic>
#include <future>
#include <list>
#include <map>
#include <memory>

namespace pierre {

using ZeroConfMap = std::map<string, ZeroConf>;
using ZeroConfFut = std::shared_future<ZeroConf>;
using ZeroConfProm = std::promise<ZeroConf>;
using ZeroConfPromMap = std::map<string, ZeroConfProm>;

class mDNS {

public:
  mDNS(io_context &io_ctx) noexcept : io_ctx(io_ctx) {}

public:
  void all_for_now(bool next_val = true) noexcept;
  static void browse(csv stype);
  void browser_remove(const string name_net) noexcept;
  static void init(io_context &io_ctx) noexcept;
  static auto port() noexcept { return PORT; }
  static void reset();
  void resolved_purge() noexcept;
  void resolver_found(const ZeroConf::Details zcd) noexcept;

  static std::shared_ptr<Service> service_ptr() noexcept;

  static void update() noexcept;
  static ZeroConfFut zservice(csv name);

private:
  void browse_impl(csv stype);
  void init_self();
  void update_impl();

public:
  // order dependent
  io_context &io_ctx;
  std::shared_ptr<Service> service;

  // order independent
  string type;
  string domain;
  string host_name;
  string error;

private:
  string _service_base;
  std::atomic_flag _all_for_now;

  ZeroConfMap zcs_map;
  ZeroConfPromMap zcs_proms;

public:
  static constexpr csv module_id{"mDNS"};
  static constexpr auto PORT{7000};
};

} // namespace pierre