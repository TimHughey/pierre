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

#include "base/conf/token.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "desk/fdecls.hpp"
#include "frame/fdecls.hpp"

#include <atomic>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <thread>
#include <vector>

namespace pierre {

namespace asio = boost::asio;

using error_code = boost::system::error_code;
using strand_ioc = asio::strand<asio::io_context::executor_type>;

class Desk {
public:
  using frame_timer = asio::steady_timer;
  using loop_clock = frame_timer::clock_type;

public:
  /// @brief Construct, start threads and being rendering.
  ///        Creates Anchor, Reel, Flusher, FX and DmxCtrl.
  ///
  ///        Begins rendering frames via DmxCtrl using the
  ///        FX determined by the availability of audio peaks.
  ///
  ///        When audio peaks are not available silence is
  ///        rendered.
  /// @param master_clock Raw pointer to MasterClock
  Desk(MasterClock *master_clock) noexcept; // see .cpp (incomplete types)
  ~Desk() noexcept;                         // see .cpp (incomplete types)

  /// @brief Reset Anchor by clearing RTP clock and timing details.
  ///        Called based on RTSP SetAnchor message
  void anchor_reset() noexcept;

  /// @brief Saves Anchor provided by RTSP SetAnchor message.
  ///        Anchor provides RTP timing information required
  ///        to accurately syncronize frame rendering with sender.
  /// @param ad xRef to AnchorData, AnchorData is moved
  void anchor_save(AnchorData &&ad) noexcept;

  /// @brief Initiate flush of frames based on RTSP flush message
  /// @param request xref to flush request, FlushInfo is moved
  void flush(FlushInfo &&request) noexcept;

  /// @brief Initiate a complete flush of all frames
  void flush_all() noexcept;

  /// @brief Handoff a raw (ciphered, encoded) for processsing
  ///        (decipher, decode) and buffering into Reel and
  ///        eventual rendering via DmxCtrl
  /// @param packet xref to raw audio packet, moved
  /// @param key const reference to the shared cipher key
  void handoff(uint8v &&packet, const uint8v &key) noexcept;

  /// @brief Set RTP timing peers from RTSP SetPeers message
  ///        Timing peers are handled off to MasterClock / nqptp
  ///        to actively participate in rendering syncronization
  /// @param p List of timing peer ip address
  void peers(const Peers &&p) noexcept;

  /// @brief Adjust rendering flag based on the RTSP SetAnchor message.
  ///
  ///        When rendering is enabled Desk will pull frames from Reel
  ///        (populated by handoff), calculate the frame state and
  ///        time to render, render the frame via FX before finally
  ///        sending the output of rendering to DmxCtrl.
  ///
  ///        When rendering is disabled Desk will send silent frames
  ///        through the same steps.
  /// @param enable boolean
  void rendering(bool enable) noexcept;

  /// @brief Resume Desk services after idle timeout (or startup).
  ///
  ///        Desk automatically reduces active threads based on an
  ///        idle timeout configured and controlled by the active FX.
  ///
  ///        Calling this function will ensure threads are resumed,
  ///        an active FX is selected, DmxCtrl is restarted and
  ///        rendering resumes.
  void resume() noexcept;

private:
  /// @brief Render a frame of audio peaks or silence
  /// @param frr frame render control state
  /// @return boolean, true if frame rendered
  bool frame_render(desk::frame_rr &frr) noexcept;

  /// @brief Primary workhorse loop for rendering frames and render management.
  ///        Frames are rendered via loop() based on their state
  ///        and sync wait which is calculated by Frame then a timer is
  ///        set to invoke loop() to repeat the process.  Sync wait
  ///        ensures frames are rendered in sync based on the sender's
  ///        timeline (see Anchor).
  void loop() noexcept;

  /// @brief Detect FX silence timeout and shutdown non-essential Desk
  ///        functions to reach an idle state.  This includes shutting down
  ///        all threads however does not deallocate Desk.
  /// @return  boolean, true if shutdown was performed
  bool shutdown_if_all_stop() noexcept;

  /// @brief Start Desk threads
  void threads_start() noexcept;

  /// @brief Stop Desk threads
  void threads_stop() noexcept;

private:
  // order dependent
  conf::token tokc;
  asio::io_context io_ctx;
  strand_ioc render_strand;
  strand_ioc flush_strand;
  frame_timer loop_timer;

  MasterClock *master_clock;
  std::unique_ptr<Anchor> anchor;
  std::unique_ptr<Reel> reel;
  std::unique_ptr<Flusher> flusher;

  // order independent
  std::atomic_flag resume_flag;
  std::atomic_flag render_flag;
  std::vector<std::jthread> threads;
  std::unique_ptr<desk::FX> active_fx{nullptr};
  std::unique_ptr<desk::DmxCtrl> dmx_ctrl;

public:
  MOD_ID("desk");
};

} // namespace pierre
