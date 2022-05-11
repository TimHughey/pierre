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

#include "core/config.hpp"
#include "core/host.hpp"
#include "core/service.hpp"

#include <array>
#include <fmt/format.h>
#include <list>
#include <memory>
#include <source_location>
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

namespace pierre {

class mDNS {
public:
  typedef std::list<AvahiEntryGroup *> Groups;

public:
  using sstream = std::stringstream;

private:
  struct Inject {
    Service &service;
  };

public:
  mDNS(const Inject &di);
  void advertise(AvahiClient *client);

  const string deviceID();

  void makePrivateKey();

  bool registerService(AvahiClient *client = nullptr);
  bool resetGroupIfNeeded();
  bool start();
  void saveGroup(AvahiEntryGroup *group);

  void update();

public:
  string _dacp_id;

  int port = 0;

  bool found = false;
  string type;
  string domain;
  string host_name;
  string error;

private:
  bool groupAddService(AvahiEntryGroup *group, auto stype, const auto &prepped_entries);
  void makePK();
  // void serviceNameCollision(AvahiEntryGroup *group);

  bool resolverNew(AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol,
                   const char *name, const char *type, const char *domain, AvahiProtocol aprotocol,
                   AvahiLookupFlags flags, AvahiServiceResolverCallback callback, void *userdata);

private:
  Service &_service;

  Groups _groups;
  string _service_base;

  static constexpr auto _port = 7000;

  // Avahi
public:
  AvahiClient *_client = nullptr;
  AvahiThreadedPoll *_tpoll = nullptr;
  AvahiServiceResolver *_resolver = nullptr;
  AvahiEntryGroupState _eg_state;
};

} // namespace pierre