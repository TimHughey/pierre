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

namespace mdns {
class Ctx;
}

class mDNS : public std::enable_shared_from_this<mDNS> {

  friend class mdns::Ctx;

private:
  mDNS() noexcept;
  static auto ptr() noexcept { return self->shared_from_this(); }

public:
  static void browse(csv stype) noexcept;
  static void init() noexcept;

  static Service &service() noexcept { return self->service_obj; }
  static void shutdown() noexcept;

  static void update() noexcept;
  static ZeroConfFut zservice(csv name) noexcept;

public:
  // order dependent
  Service service_obj;
  std::unique_ptr<mdns::Ctx> ctx;

  // static class data
  static std::shared_ptr<mDNS> self;

public:
  static constexpr csv module_id{"mdns.base"};
};

} // namespace pierre