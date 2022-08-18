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
#include "mdns/mdns.hpp"

#include <latch>
#include <math.h>
#include <ranges>

namespace pierre {
namespace shared {
std::optional<shDesk> __desk;
} // namespace shared

struct async_read_msg_impl {
  tcp_socket &socket;
  uint8v &buff;
  enum { starting, msg_content, finished } state = starting;

  template <typename Self> void operator()(Self &self, error_code ec = {}, size_t n = 0) {

    switch (state) {
    case starting:
      state = msg_content;
      socket.async_read_some(asio::buffer(buff.data(), sizeof(uint16_t)),
                             asio::bind_executor(socket.get_executor(), std::move(self)));
      break;

    case msg_content:
      if (!ec) {
        state = finished;
        socket.async_read_some(asio::buffer(buff.data(), msg_len()),
                               asio::bind_executor(socket.get_executor(), std::move(self)));
      } else {
        self.complete(ec, n);
      }
      break;

    case finished:
      self.complete(ec, n);
      break;
    }
  }

private:
  uint16_t msg_len() {
    uint16_t *msg_len_ptr = (uint16_t *)buff.data();
    return ntohs(*msg_len_ptr);
  }

  void log(const error_code &ec, size_t n) {
    __LOG0(LCOL01 " state={} len={} reason={}\n", Desk::moduleID(), "IMPL", state, n,
           ec.message());
  }
};

template <typename CompletionToken>
auto async_read_msg(tcp_socket &socket, uint8v &buff, CompletionToken &&token) ->
    typename asio::async_result<typename std::decay<CompletionToken>::type,
                                void(error_code, size_t)>::return_type {
  return asio::async_compose<CompletionToken, void(error_code, size_t)>(
      async_read_msg_impl(socket, buff), token, socket);
}

// must be defined in .cpp to hide mdns
Desk::Desk()
    : local_strand(io_ctx),                         // local strand to serialize work
      release_timer(io_ctx),                        // controls when frame is sent
      active_fx(fx_factory::create<fx::Silence>()), // active FX initializes to Silence
      latency(5ms),                                 // ruth latency (default)
      zservice(mDNS::zservice("_ruth._tcp")),       // get the remote service, if available
      work_buff(1024),                              // pre-allocate work buff
      guard(std::make_shared<work_guard>(io_ctx.get_executor())) {}

// static creation, access to instance
void Desk::init() { // static
  auto desk = shared::desk().emplace(new Desk());

  std::latch threads_latch(DESK_THREADS);
  desk->thread_main = Thread([=, &threads_latch](std::stop_token token) {
    desk->stop_token = token;
    name_thread("Desk", 0);

    // note: work guard created by constructor
    for (auto n = 1; n < DESK_THREADS; n++) { // main thread is 0s
      desk->threads.emplace_back([=, &threads_latch] {
        name_thread("Desk", n);
        threads_latch.count_down();
        desk->io_ctx.run();
      });
    }

    threads_latch.count_down();
    desk->io_ctx.run();
  });

  threads_latch.wait(); // wait for all threads to start
}

// General API
void Desk::handle_frame(shFrame frame) {
  auto msg = desk::Msg::create(frame);

  if ((active_fx->matchName(fx::SILENCE)) && !frame->silence()) {
    active_fx = fx_factory::create<fx::MajorPeak>();
  }

  active_fx->render(frame, msg);

  // the socket is only created if it doesn't exist
  // connect() returns true when the connection is ready
  if (connect()) {
    if (data_socket.has_value() && data_endpoint.has_value()) {
      msg->transmit(data_socket.value(), data_endpoint.value());
    }
  }
}

void Desk::reset_connection() {
  [[maybe_unused]] error_code ec; // generic and unchecked
  error_code shut_ec;
  error_code close_ec;

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

void Desk::setup_connection() {
  async_read_msg(
      *ctrl_socket, work_buff,
      asio::bind_executor(local_strand, [self = ptr()](const error_code ec, size_t bytes) {
        if (!ec) {
          StaticJsonDocument<2048> doc;

          if (auto err = deserializeMsgPack(doc, self->work_buff.data(), bytes); err) {
            __LOG0(LCOL01 " failed, reason={}\n", moduleID(), "DESERAILIZE", err.c_str());
            self->reset_connection();
          } else {
            uint16_t data_port = doc["data_port"].as<uint16_t>();
            __LOG0(LCOL01 " remote udp data_port={}\n", moduleID(), "SETUP", data_port);

            self->data_socket.emplace(self->io_ctx).open(ip_udp::v4());
            self->data_endpoint.emplace(asio::ip::make_address_v4(self->zservice->address()),
                                        data_port);
          }
        }
      }));
}

void Desk::watch_connection() {
  ctrl_socket->async_wait(    // wait for a specific event
      tcp_socket::wait_error, // any error on the socket
      asio::bind_executor(local_strand, [self = shared_from_this()](error_code ec) {
        __LOG0(LCOL01 " socket shutdown reason={}\n", moduleID(), "WATCH", ec.message());
        self->reset_connection();
      }));
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
