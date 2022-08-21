/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include "desk/desk.hpp"
// #include "desk/fx/colorbars.hpp"
// #include "desk/fx/leave.hpp"
#include "ArduinoJson.hpp"
#include "base/uint8v.hpp"
#include "desk/fx.hpp"
#include "desk/fx/majorpeak.hpp"
#include "desk/fx/silence.hpp"
#include "desk/msg.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/opts.hpp"
#include "io/async_msg.hpp"
#include "mdns/mdns.hpp"
#include "rtp_time/anchor.hpp"
#include "spooler/spooler.hpp"

#include <future>
#include <latch>
#include <math.h>
#include <ranges>

namespace pierre {
namespace desk { // instance
std::unique_ptr<Desk> i;
} // namespace desk

Desk *idesk() { return desk::i.get(); }

// must be defined in .cpp to hide mdns
Desk::Desk(const Nanos &lead_time)
    : lead_time(lead_time),                         // despool frame lead time
      frame_strand(io_ctx),                         // local strand to serialize work
      frame_timer(io_ctx),                          // controls when frame is despooled
      conn_strand(io_ctx),                          // serialize connection and msg exchange
      release_timer(io_ctx),                        // controls when frame is sent
      active_fx(fx_factory::create<fx::Silence>()), // active FX initializes to Silence
      latency(5ms),                                 // ruth latency (default)
      zservice(mDNS::zservice("_ruth._tcp")),       // get the remote service, if available
      work_buff(1024),                              // pre-allocate work buff
      guard(io_ctx.get_executor()),                 // prevent io_ctx from ending
      play_mode(NOT_PLAYING),                       // render or don't render
      stats(io_ctx, 10s) {}

// general API
void Desk::adjust_play_mode(csv next_mode) {
  play_mode = next_mode;

  if (play_mode == PLAYING) {
    frame_next(); // starts despooling frames
    stats.async_report(5ms);
  } else {
    frame_timer.cancel();
    stats.cancel();
  }
}

bool Desk::connect() {
  if (zservice && !ctrl_socket.has_value()) {
    __LOG0(LCOL01 " attempting connect\n", moduleID(), "CONNECT");

    ctrl_socket.emplace(io_ctx);
    ctrl_endpoint.emplace(asio::ip::make_address_v4(zservice->address()), zservice->port());

    asio::async_connect(            // async connect
        *ctrl_socket,               // this socket
        std::array{*ctrl_endpoint}, // to this endpoint
        asio::bind_executor(        // serialize with
            conn_strand, [this](const error_code ec, const tcp_endpoint endpoint) {
              if (!ec) {
                ctrl_socket->set_option(ip_tcp::no_delay(true));
                endpoint_connected.emplace(endpoint);

                log_connected();
                watch_connection();
                setup_connection();

              } else {
                reset_connection();
              }
            }));
  } else if (!zservice) {
    zservice = mDNS::zservice("_ruth._tcp");
  }

  return endpoint_connected.has_value() && ctrl_socket->is_open();
}

void Desk::frame_render(shFrame frame) {
  auto msg = desk::Msg::create(frame);

  if ((active_fx->matchName(fx::SILENCE)) && !frame->silence()) {
    active_fx = fx_factory::create<fx::MajorPeak>();
  }

  active_fx->render(frame, msg);

  // the socket is only created if it doesn't exist
  // connect() returns true when the connection is ready
  if (connected()) {
    msg->transmit(data_socket.value(), data_endpoint.value());
  }
}

void Desk::frame_next(Nanos sync_wait, Nanos lag) {
  static csv fn_id = "FRAME_NEXT";

  if (playing()) {
    connect();

    frame_timer.expires_after(sync_wait);
    frame_timer.async_wait( // wait the required sync duration before next frame
        asio::bind_executor(frame_strand, [=, this](const error_code &ec) mutable {
          if (!ec) {
            Elapsed elapsed;

            // waits for future
            auto future = ispooler()->next_frame(lead_time);
            auto status = future.wait_for(sync_wait);
            auto frame = status == std::future_status::ready ? future.get() : shFrame();

            if (frame && frame->playable()) {
              sync_wait = frame->calc_sync_wait(lag);

              // mark played and immediately send the frame to Desk
              frame_release(frame);
              stats.handled++;
            } else {
              sync_wait = Nanos(lead_time / 2);
              stats.none++;
            }

            log_despooled(frame, elapsed);

            // use sync_wait calculated above to wait for the next frame and the
            // elapsed time as the lag
            frame_next(sync_wait, elapsed());
          } else {
            __LOG0(LCOL01 " playing={} reason={}\n", moduleID(), fn_id, playing(), ec.message());
          }
        }));
  }
}

void Desk::frame_release(shFrame frame) {
  frame->mark_played();

  // recalc the sync wait to account for lag
  auto sync_wait = frame->calc_sync_wait(latency);

  sync_wait = (sync_wait < Nanos::zero()) ? Nanos::zero() : std::move(sync_wait);

  // schedule the release
  release_timer.expires_after(sync_wait);
  release_timer.async_wait([this, frame = frame](const error_code &ec) {
    if (!ec) {
      frame_render(frame);
    }
  });
}

void Desk::init(const Nanos &lead_time) { // static instance creation
  if (auto self = desk::i.get(); !self) {

    self = new Desk(lead_time);
    desk::i = std::move(std::unique_ptr<Desk>(self));

    __LOG0(LCOL01 " ptr={} sizeof=%u", moduleID(), "INIT", fmt::ptr(self), sizeof(Desk));

    std::latch latch(DESK_THREADS);

    // note: work guard created by constructor
    for (auto n = 0; n < DESK_THREADS; n++) { // main thread is 0s
      self->threads.emplace_back([=, &latch] {
        name_thread(TASK_NAME, n);
        latch.count_down();
        self->io_ctx.run();
      });
    }

    // caller thread waits until all tasks are started
    latch.wait();
  }

  // ensure spooler is started
  Spooler::init();
}

void Desk::reset_connection() {
  [[maybe_unused]] error_code ec; // generic and unchecked
  error_code shut_ec;
  error_code close_ec;

  frame_timer.cancel(ec);
  release_timer.cancel(ec);

  if (ctrl_socket.has_value()) {
    ctrl_socket->cancel(ec);
    ctrl_socket->shutdown(tcp_socket::shutdown_both, shut_ec);
    ctrl_socket->close(close_ec);

    __LOG0(LCOL01 " shutdown_reason={} close_reason={}\n", moduleID(), "RESET", shut_ec.message(),
           close_ec.message());
  }

  if (data_socket.has_value()) {
    data_socket->cancel(ec);
    data_socket->shutdown(udp_socket::shutdown_both, ec);
    data_socket->close(ec);
  }

  ctrl_endpoint.reset();
  endpoint_connected.reset();
  data_endpoint.reset();

  ctrl_socket.reset();
  data_socket.reset();
}

void Desk::save_anchor_data(anchor::Data data) {
  auto anchor = Anchor::ptr().get();
  anchor->save(data);

  adjust_play_mode(data.play_mode());
}

void Desk::setup_connection() {

  async_read_msg(
      *ctrl_socket, work_buff,
      asio::bind_executor(conn_strand, [this](const error_code ec, size_t bytes) {
        if (!ec) {
          StaticJsonDocument<2048> doc;

          if (auto err = deserializeMsgPack(doc, work_buff.data(), bytes); err) {
            __LOG0(LCOL01 " failed, reason={}\n", moduleID(), "DESERAILIZE", err.c_str());
            reset_connection();
          } else {
            uint16_t data_port = doc["data_port"].as<uint16_t>();
            __LOG0(LCOL01 " remote udp data_port={}\n", moduleID(), "SETUP", data_port);

            data_socket.emplace(io_ctx).open(ip_udp::v4());
            data_endpoint.emplace(asio::ip::make_address_v4(zservice->address()), data_port);
          }
        }
      }));
}

void Desk::watch_connection() {
  ctrl_socket->async_wait(    // wait for a specific event
      tcp_socket::wait_error, // any error on the socket
      asio::bind_executor(conn_strand, [this](error_code ec) {
        __LOG0(LCOL01 " socket shutdown reason={}\n", moduleID(), "WATCH", ec.message());
        reset_connection();
      }));
}

// misc debug and logging API
void Desk::log_despooled(shFrame frame, Elapsed &elapsed) {
  static uint64_t no_playable = 0;
  static uint64_t playable = 0;

  asio::defer(io_ctx, [=, this]() {
    string msg;
    auto w = std::back_inserter(msg);

    if (frame) {
      no_playable = 0;
      playable++;
      if ((frame->sync_wait < 10ms) || ((playable % 1000) == 0)) {
        fmt::format_to(w, " seq_num={} sync_wait={}", frame->seq_num,
                       pe_time::as_duration<Nanos, MillisFP>(frame->sync_wait));
      }
    } else {
      playable = 0;
      no_playable++;
      if ((no_playable == 1) || ((no_playable % 100) == 0)) {
        fmt::format_to(w, " no playable frames");
      }
    }

    if (elapsed() >= 3ms) {
      fmt::format_to(w, " elapsed={:0.2}", elapsed.as<MillisFP>());
    }

    if (!msg.empty()) {
      __LOG0(LCOL01 "{}\n", moduleID(), "DESPOOLED", msg);
    }
  });
}

/*
void Desk::leave() {
  // auto x = State::leavingDuration<seconds>().count();
  {
    lock_guard lck(_active.mtx);
    _active.fx = make_shared<Leave>();
  }

  this_thread::sleep_for(State::leavingDuration<milliseconds>());
  State::shutdown();
}

void Desk::stream() {
  while (State::isRunning()) {
    // nominal condition:
    // sound is present and MajorPeak is active
    if (_active.fx->matchName("MajorPeak"sv)) {
      // if silent but not yet reached suspended start Leave
      if (State::isSilent()) {
        if (!State::isSuspended()) {
          _active.fx = make_shared<Leave>();
        }
      }
      goto delay;
    }
    // catch anytime we're coming out of silence or suspend
    // ensure that MajorPeak is running when not silent and not suspended
    if (!State::isSilent() && !State::isSuspended()) {
      _active.fx = make_shared<MajorPeak>();
      goto delay;
    }
    // transitional condition:
    // there is no sound present however the timer for suspend hasn't elapsed
    if (_active.fx->matchName("Leave"sv)) {
      // if silent and have reached suspended, start Silence
      if (State::isSilent() && State::isSuspended()) {
        _active.fx = make_shared<Silence>();
      }
    }

    goto delay;

  delay:
    this_thread::sleep_for(seconds(1));
  }
}
*/

} // namespace pierre
