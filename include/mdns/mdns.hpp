/*
Pierre - Custom audio processing for light shows at Wiss Landing
Copyright (C) 2022  Tim Hughey

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <array>
#include <fmt/format.h>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-client/lookup.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/timeval.h>

#include "core/config.hpp"
#include "core/host.hpp"
#include "service/airplay.hpp"
#include "service/raop.hpp"

namespace pierre {

// forward decl for typedef
class mDNS;

typedef std::shared_ptr<mDNS> smDNS;

class mDNS : std::enable_shared_from_this<mDNS> {
public:
  typedef std::array<uint8_t, 32> PkBytes;
  typedef std::list<mdns::Service> ServiceList;
  typedef std::list<AvahiEntryGroup *> Groups;

public:
  using string = std::string;
  using sstream = std::stringstream;

public:
  struct Opts {
    sHost host;
    const string &service_base;
    const string &firmware_vsn;
  };

public:
  void advertise(AvahiClient *client);

  [[nodiscard]] static smDNS create(const Opts &opts) {
    // Not using std::make_shared<T> because the c'tor is private.
    return smDNS(new mDNS(opts));
  }

  smDNS getPtr() { return shared_from_this(); }

  void makePrivateKey();

  bool registerService(AvahiClient *client = nullptr);
  bool start();
  void saveGroup(AvahiEntryGroup *group) { _groups.emplace_back(group); }

public:
  string _dacp_id;

  int port = 0;

  bool found = false;
  string type;
  string domain;
  string host_name;
  string error;

private:
  mDNS(const Opts &opts);
  void makeServices();

private: // Callbacks
  static void cbBrowse(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol,
                       AvahiBrowserEvent event, const char *name, const char *type,
                       const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
                       void *userdata);

  static void cbClient(AvahiClient *client, AvahiClientState state, void *userdata);

  static void cbEntryGroup(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata);

  static void cbResolve(AvahiServiceResolver *r, [[maybe_unused]] AvahiIfIndex interface,
                        [[maybe_unused]] AvahiProtocol protocol, AvahiResolverEvent event,
                        const char *name, const char *type, const char *domain,
                        [[maybe_unused]] const char *host_name,
                        [[maybe_unused]] const AvahiAddress *address, uint16_t port,
                        [[maybe_unused]] AvahiStringList *txt,
                        [[maybe_unused]] AvahiLookupResultFlags flags, void *userdata);

private: // error reporting helpers
  static string error_string(AvahiClient *client);
  static string error_string(AvahiEntryGroup *group);
  static string error_string(AvahiServiceBrowser *browser);

private:
  bool groupAddService(AvahiEntryGroup *group, const auto &service, const auto &prepped_entries);
  void makePK();
  // void serviceNameCollision(AvahiEntryGroup *group);

  bool resolverNew(AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol,
                   const char *name, const char *type, const char *domain, AvahiProtocol aprotocol,
                   AvahiLookupFlags flags, AvahiServiceResolverCallback callback, void *userdata);

private:
  ServiceList _services;
  Groups _groups;
  PkBytes _pk_bytes;
  string _pk;

  sHost _host;
  string _service_base;
  string _firmware_vsn;

  static constexpr auto _port = 7000;

  // Avahi
public:
  AvahiClient *_client = nullptr;
  AvahiThreadedPoll *_tpoll = nullptr;
  AvahiServiceResolver *_resolver = nullptr;
  AvahiEntryGroupState _eg_state;
};

} // namespace pierre