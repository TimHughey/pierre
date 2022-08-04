/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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

#pragma once

#include "base/elapsed.hpp"
#include "base/typical.hpp"
#include "desk/dmx.hpp"
#include "desk/fx.hpp"
#include "desk/headunit.hpp"
#include "dsp/peak_info.hpp"
#include "io/io.hpp"
#include "mdns/mdns.hpp"

#include <exception>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

namespace pierre {
namespace {
namespace ranges = std::ranges;
}

class Desk;
typedef std::shared_ptr<Desk> shDesk;

namespace shared {
extern std::optional<shDesk> __desk;
constexpr std::optional<shDesk> &desk() { return __desk; }
} // namespace shared

class Desk : public std::enable_shared_from_this<Desk> {
private:
  typedef std::vector<shHeadUnit> Units;

public:
  template <typename T> static shFX createFX() {
    return std::static_pointer_cast<T>(std::make_shared<T>());
  }

  template <typename T> static std::shared_ptr<T> createUnit(auto opts) {
    return ptr()->addUnit(opts);
  }

  template <typename T> static std::shared_ptr<T> derivedFX(csv name) {
    return std::static_pointer_cast<T>(ptr()->active_fx);
  }

  template <typename T> static std::shared_ptr<T> derivedUnit(csv name) {
    return std::static_pointer_cast<T>(unit(name));
  }

private:
  Desk(io_context &io_ctx); // must be definited in .cpp to hide FX includes

public:
  // static create, access to Desk, FX and Unit
  static shDesk create(io_context &io_ctx);
  static shDesk ptr() { return shared::desk().value()->shared_from_this(); }
  static void reset() { shared::desk().reset(); }
  static constexpr csv moduleID() { return module_id; }

public:
  static shFX activeFX() { return ptr()->active_fx; }

  template <typename T> static shFX activateFX(csv &fx_name) {
    return ptr()->activate<T>(fx_name);
  }

  static void dark() {
    forEach([](shHeadUnit unit) { unit->dark(); });
  }

  void handlePeaks(const PeakInfo &peak_info);
  static void nextPeaks(PeakInfo peak_info) {
    auto self = ptr();

    asio::post(self->local_strand, [self = ptr(), peak_info = std::move(peak_info)]() {
      self->handlePeaks(peak_info);
    });
  }

  static void leave() {
    forEach([](shHeadUnit unit) { unit->leave(); });
  }

  bool silence() const { return active_fx && active_fx->matchName(fx::SILENCE); }

  static shHeadUnit unit(csv name) {
    const auto &units = ptr()->units;

    auto unit = ranges::find_if(units, [name = name](shHeadUnit u) { //
      return name == u->unitName();
    });

    if (unit != units.end()) [[likely]] {
      return *unit;
    } else {
      static string msg;
      fmt::format_to(std::back_inserter(msg), "unit [{}] not found", name);
      throw(std::runtime_error(msg.c_str()));
    }
  }

private:
  template <typename T> shFX activate(csv &fx_name) {
    if (active_fx.use_count() && !active_fx->matchName(fx_name)) {
      // shared_ptr empty or name doesn't match, create the FX
      active_fx = createFX<T>();
    }

    return active_fx;
  }

  template <typename T> std::shared_ptr<T> addUnit(const auto opts) {
    return std::static_pointer_cast<T>(units.emplace_back(std::make_shared<T>(opts)));
  }

  static void forEach(auto func) { ranges::for_each(ptr()->units, func); }

  bool connect() {
    __LOGX(LCOL01 " zservice.use_count={} socket2.has_value()={}\n", moduleID(), "CONNECT",
           zservice.use_count(), socket2.has_value());

    if (zservice && !socket2.has_value()) {
      __LOG0(LCOL01 " attempting connect\n", moduleID(), "CONNECT");

      socket2.emplace(io_ctx);
      endpoint2.emplace(asio::ip::make_address_v4(zservice->address()), zservice->port());

      asio::async_connect(        // async connect
          *socket2,               // this socket
          std::array{*endpoint2}, // to this endpoint
          asio::bind_executor(    //
              local_strand, [self = ptr()](const error_code ec, const tcp_endpoint endpoint) {
                if (!ec) {

                  self->socket2->set_option(ip_tcp::no_delay(true));
                  self->endpoint_connected.emplace(endpoint);

                  __LOG0(LCOL01 " connected handle={} {}:{} -> {}:{}\n", moduleID(), "CONNECT",
                         self->socket2->native_handle(),
                         self->socket2->local_endpoint().address().to_string(),
                         self->socket2->local_endpoint().port(),
                         self->endpoint_connected->address().to_string(),
                         self->endpoint_connected->port());

                  self->socket2->async_wait(  //
                      tcp_socket::wait_error, //
                      asio::bind_executor(self->local_strand, [=]([[maybe_unused]] error_code ec) {
                        error_code shutdown_ec;
                        error_code close_ec;

                        self->socket2->shutdown(tcp_socket::shutdown_both, shutdown_ec);
                        self->socket2->close(close_ec);
                        self->socket2.reset();
                        self->endpoint_connected.reset();

                        __LOG0(LCOL01 " socket error shutdown_reason={} close_reason={}\n",
                               moduleID(), "ASYNC_WAIT", shutdown_ec.message(),
                               close_ec.message());
                      }));

                } else {
                  self->socket2.reset();
                  self->endpoint2.reset();
                  self->endpoint_connected.reset();
                  __LOG0(LCOL01 " connect failed reason={}\n", moduleID(), "CONNECT",
                         ec.message());
                }
              }));
    } else if (!zservice) {
      zservice = mDNS::zservice(("_ruth._tcp"));
    }

    return endpoint_connected.has_value() && socket2->is_open();
  }

  bool createSocket() {
    if (zservice && !socket.has_value()) {
      socket.emplace(io_ctx).open(ip_udp::v4());

      Port remote_port = zservice->txtVal<uint32_t>("msg_port");
      endpoint.emplace(asio::ip::make_address_v4(zservice->address()), remote_port);

      __LOG0(LCOL01 " created socket handle={}\n", moduleID(), "CREATE_SOCKET",
             socket->native_handle());
    } else if (!zservice) {
      zservice = mDNS::zservice(("_ruth._tcp"));
    }

    return socket.has_value() && socket->is_open();
  }

private:
  // order dependent
  io_context &io_ctx;
  strand local_strand;
  shFX active_fx;
  shZeroConfService zservice;

  // order independent
  std::optional<udp_socket> socket;
  std::optional<udp_endpoint> endpoint;
  std::optional<tcp_socket> socket2;
  std::optional<tcp_endpoint> endpoint2;
  std::optional<tcp_endpoint> endpoint_connected;

  Units units;

  static constexpr csv module_id = "DESK";
};

} // namespace pierre
