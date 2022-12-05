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

class Desk : public std::enable_shared_from_this<Desk> {
private:
  Desk() noexcept; // must be defined in .cpp to hide FX includes

public:
  static void init() noexcept;
  static void shutdown() noexcept;

private:
  void frame_loop(Nanos wait = InputInfo::lead_time_min) noexcept;
  void init_self() noexcept;

  // misc debug

  // log_frame_timer_error: return true if ec == success
  bool log_frame_timer(const error_code &ec, csv fn_id) const noexcept;
  void log_init(int num_threads) const noexcept;

private:
  // order dependent
  io_context io_ctx;
  strand frame_strand;
  Nanos frame_last;
  work_guard_t guard;
  std::atomic_bool loop_active;

  // order independent
  bool fx_finished{true};
  Threads threads;
  stop_tokens tokens;

public:
  static constexpr csv module_id{"DESK"};
  static constexpr auto TASK_NAME{"Desk"};
};

} // namespace pierre
