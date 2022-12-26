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

#include "base/input_info.hpp"
#include "base/io.hpp"
#include "base/pet.hpp"
#include "base/threads.hpp"
#include "base/types.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <optional>

namespace pierre {

// forward decls to hide implementation details
class DmxCtrl;
class FX;

class Desk : public std::enable_shared_from_this<Desk> {
private:
  Desk(io_context &io_ctx_caller) noexcept; // must be defined in .cpp to hide FX includes
  auto ptr() noexcept { return shared_from_this(); }

public:
  static void init(io_context &io_ctx_caller) noexcept; // in .cpp to hide DmxCtrl()

  void shutdown() noexcept;

private:
  void frame_loop(Nanos wait = InputInfo::lead_time_min) noexcept;

private:
  // order dependent
  io_context &io_ctx_caller;
  io_context io_ctx;
  strand frame_strand;
  work_guard guard;
  std::atomic_bool loop_active;

  // order independent
  bool fx_finished{true};
  static std::shared_ptr<Desk> self;
  std::shared_ptr<DmxCtrl> dmx_ctrl;
  std::shared_ptr<FX> active_fx;
  Threads threads;

public:
  static constexpr csv module_id{"DESK"};
  static constexpr auto TASK_NAME{"desk"};
};

} // namespace pierre
