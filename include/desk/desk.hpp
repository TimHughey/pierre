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
#include "base/pet.hpp"
#include "base/types.hpp"
#include "frame/racked.hpp"
#include "io/io.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <optional>

namespace pierre {

// forward decls to hide implementation details
class DmxCtrl;
class FX;
class Racked;

class Desk {

public:
  Desk() noexcept; // must be defined in .cpp to hide FX includes
  ~Desk() noexcept;

  void flush(FlushInfo &&request) noexcept {
    if (racked.has_value()) {
      racked->flush(std::forward<FlushInfo>(request));
    }
  }

  void flush_all() noexcept {
    if (racked.has_value()) racked->flush_all();
  }

  void handoff(uint8v &&packet, const uint8v &key) noexcept {
    if (racked.has_value()) racked->handoff(std::forward<uint8v>(packet), key);
  }

  void resume() noexcept;

  void spool(bool enable = true) noexcept {
    if (racked.has_value()) racked->spool(enable);
  }

  void standby() noexcept;

private:
  void frame_loop() noexcept;

private:
  // order dependent
  io_context io_ctx;
  strand frame_strand;
  steady_timer frame_timer;
  work_guard guard;
  std::atomic_bool loop_active{false};
  const int thread_count;
  std::shared_ptr<std::latch> startup_latch;
  std::shared_ptr<std::latch> shutdown_latch;
  std::atomic_bool started;

  // order independent
  bool fx_finished{true};
  std::optional<Racked> racked;

  std::shared_ptr<DmxCtrl> dmx_ctrl{nullptr};
  std::unique_ptr<FX> active_fx{nullptr};

public:
  static constexpr csv module_id{"desk"};
  static constexpr auto TASK_NAME{"desk"};
};

} // namespace pierre
