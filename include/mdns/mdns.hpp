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

namespace pierre {

class mDNS {
public:
  using string = std::string;
  using sstream = std::stringstream;

public:
  mDNS(ConfigPtr cfg);

  bool registerService(AvahiClient *client = nullptr);
  bool start();

  AvahiStringList *makeBonjour();

public:
  string _dacp_id;
  string _service_name;
  int port = 0;

  bool found = false;
  string type;
  string domain;
  string host_name;
  string error;

public:
  string regType{"_raop._tcp"};

private: // Callbacks
  static void cbBrowse(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol,
                       AvahiBrowserEvent event, const char *name, const char *type, const char *domain,
                       AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void *userdata);

  static void cbClient(AvahiClient *client, AvahiClientState state, void *userdata);

  static void cbEntryGroup(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata);

  static void cbResolve(AvahiServiceResolver *r, [[maybe_unused]] AvahiIfIndex interface,
                        [[maybe_unused]] AvahiProtocol protocol, AvahiResolverEvent event, const char *name,
                        const char *type, const char *domain, [[maybe_unused]] const char *host_name,
                        [[maybe_unused]] const AvahiAddress *address, uint16_t port,
                        [[maybe_unused]] AvahiStringList *txt, [[maybe_unused]] AvahiLookupResultFlags flags,
                        void *userdata);

private: // error reporting helpers
  static string error_string(AvahiClient *client);
  static string error_string(AvahiEntryGroup *group);
  static string error_string(AvahiServiceBrowser *browser);

private:
  void serviceNameCollision(AvahiEntryGroup *group);

  bool resolverNew(AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol, const char *name,
                   const char *type, const char *domain, AvahiProtocol aprotocol, AvahiLookupFlags flags,
                   AvahiServiceResolverCallback callback, void *userdata);

private:
  ConfigPtr _cfg;

  // Avahi
public:
  AvahiClient *_client = nullptr;
  AvahiEntryGroup *_group = nullptr;
  AvahiThreadedPoll *_tpoll = nullptr;
  AvahiServiceResolver *_resolver = nullptr;
  AvahiEntryGroupState _eg_state;
};

} // namespace pierre