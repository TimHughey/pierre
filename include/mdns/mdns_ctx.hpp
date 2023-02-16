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

#include "base/types.hpp"
#include "service.hpp"
#include "zservice.hpp"

#include <atomic>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/timeval.h>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>

namespace pierre {
namespace mdns {

using Groups = std::list<AvahiEntryGroup *>;
using Entries = std::vector<string>;
using ZeroConfServiceList = std::list<ZeroConf>;

class Ctx {
public:
  Ctx(const string &stype, Service &service, Port receiver_port) noexcept;
  ~Ctx() noexcept;

  void browse(csv stype) noexcept;

  void update(Service &service) noexcept;

  ZeroConfFut zservice(csv name) noexcept;

private:
  /// @brief Advertise the service, invoked by cb_client() when client connection ready
  /// @param service shared_ptr to service to advertise
  void advertise(Service &service) noexcept;

  void all_for_now(bool next_val) noexcept;

  void browse_remove(const string name) noexcept;

  // avahi callbacks
  static void cb_browse(AvahiServiceBrowser *b, AvahiIfIndex iface, AvahiProtocol protocol,
                        AvahiBrowserEvent event, ccs name, ccs type, ccs domain,
                        AvahiLookupResultFlags flags, void *d);
  static void cb_client(AvahiClient *client, AvahiClientState state, void *d);
  static void cb_entry_group(AvahiEntryGroup *group, AvahiEntryGroupState state, void *d);
  static void cb_resolve(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol,
                         AvahiResolverEvent event, ccs name, ccs type, ccs domain, ccs host_name,
                         const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
                         AvahiLookupResultFlags flags, void *d);

  template <typename T> static string error_string(T t) {
    using U = std::remove_pointer<T>::type;

    if constexpr (std::is_same_v<U, AvahiClient>) {
      return string(avahi_strerror(avahi_client_errno(t)));
    } else if constexpr (std::is_same_v<U, AvahiServiceBrowser>) {
      return error_string(avahi_service_browser_get_client(t));
    } else if constexpr (std::is_same_v<U, AvahiEntryGroup>) {
      return error_string(avahi_entry_group_get_client(t));
    } else {
      static_assert(always_false_v<U>, "unhandled Avahi type");
    }
  }

  static zc::TxtList make_txt_list(AvahiStringList *txt) noexcept;

  void lock() noexcept { avahi_threaded_poll_lock(tpoll); }

  void resolved(const ZeroConf::Details zcd) noexcept;

  void unlock() noexcept { avahi_threaded_poll_unlock(tpoll); }

public:
  // order dependent
  const string stype;
  const Port receiver_port;
  std::atomic_bool all_for_now_state;
  std::atomic_bool client_running;
  std::atomic_bool threaded_poll_quit;
  AvahiThreadedPoll *tpoll{nullptr};

  // order independent
  string err_msg;
  string domain;

  AvahiClient *client{nullptr};

  AvahiEntryGroupState entry_group_state{AVAHI_ENTRY_GROUP_UNCOMMITED};
  std::optional<AvahiEntryGroup *> entry_group;

  // pending and resolved names
  ZeroConfMap zcs_map;
  ZeroConfPromMap zcs_proms;

  static constexpr csv module_id{"mdns.ctx"};
  static constexpr csv thread_name{"mdns"};
};

} // namespace mdns
} // namespace pierre