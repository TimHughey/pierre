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

#include <array>
#include <avahi-client/publish.h>
#include <fmt/format.h>

#include <iostream>
#include <list>
#include <memory>
#include <pthread.h>

#include <stdlib.h>

#include "core/service.hpp"
#include "mdns.hpp"
#include "mdns/callbacks.hpp"

namespace pierre {

using namespace std;
using namespace fmt;

using enum service::Key;
using enum service::Type;

typedef std::vector<string> PreppedEntries;

mDNS::mDNS(sService service) : _service(service) {}

void mDNS::advertise(AvahiClient *client) {
  _client = client;

  if (_client == nullptr) {
    fmt::print("mDNS advertise(): client={}\n", fmt::ptr(client));
    return;
  }

  if (_groups.size() == 1) {
    fmt::print("mDNS advertise(): services already advertised: ");
    return;
  }

  // advertise the services
  // build and commit a single group containing two services:
  //  _airplay._tcp string list
  //  _raop._tcp string list

  auto group = avahi_entry_group_new(_client, mdns::cbEntryGroup, this);

  for (const auto stype : std::array{AirPlayTCP, RaopTCP}) {
    PreppedEntries entries;
    auto kvmap = _service->keyValList(stype);

    for (const auto &[key, val] : *kvmap) {
      fmt::basic_memory_buffer<char, 128> sl_entry;

      fmt::format_to(sl_entry, "{}={}", key, val);
      entries.emplace_back(fmt::to_string(sl_entry));
    }

    groupAddService(group, stype, entries);
  }

  auto rc = avahi_entry_group_commit(group);

  if (rc != AVAHI_OK) {
    fmt::print("advertise(): Unhandled error={}\n", avahi_strerror(rc));
  }
}

const string mDNS::deviceID() {
  const auto &[__unused_key, val_str] = _service->fetch(apDeviceID);

  return string(val_str);
}

bool mDNS::groupAddService(AvahiEntryGroup *group, auto stype, const auto &prepped_entries) {
  constexpr auto iface = AVAHI_IF_UNSPEC;
  constexpr auto proto = AVAHI_PROTO_UNSPEC;
  constexpr auto pub_flags = (AvahiPublishFlags)0;

  const auto [reg_type, name] = _service->nameAndReg(stype);

  std::vector<const char *> string_pointers;
  for (auto &entry : prepped_entries) {
    string_pointers.emplace_back(entry.c_str());
  }

  auto sl = avahi_string_list_new_from_array(string_pointers.data(), string_pointers.size());
  auto rc = avahi_entry_group_add_service_strlst(group, iface, proto, pub_flags, name, reg_type,
                                                 NULL, NULL, _port, sl);

  if (rc == AVAHI_ERR_COLLISION) {
    fmt::print("AirPlay2 name already in use\n");
    avahi_string_list_free(sl);
    return false;
  }

  if (rc != AVAHI_OK) {
    fmt::print("groupAddService(): Unhandled error={}\n", avahi_strerror(rc));
  }

  return (rc == AVAHI_OK) ? true : false;
}

bool mDNS::resolverNew(AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol,
                       const char *name, const char *type, const char *domain,
                       [[maybe_unused]] AvahiProtocol aprotocol,
                       [[maybe_unused]] AvahiLookupFlags flags,
                       [[maybe_unused]] AvahiServiceResolverCallback callback, void *userdata) {
  _resolver = avahi_service_resolver_new(client, interface, protocol, name, type, domain,
                                         AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)9, mdns::cbResolve,
                                         userdata);

  if (_resolver == nullptr) {
    error = format("BROWSER RESOLVE failed, service[{}] {} ", name, mdns::error_string(client));
    return false;
  }

  return true;
}

void mDNS::saveGroup(AvahiEntryGroup *group) {
  if (_groups.size() > 0) {
    fmt::print("mDNS::saveGroup(): purging previous groups: ");

    for (const auto &old : _groups) {
      fmt::print("{} ", fmt::ptr(old));
    }

    fmt::print("\n");
    _groups.clear();
  }

  _groups.emplace_back(group);
}

bool mDNS::start() {
  _tpoll = avahi_threaded_poll_new();
  auto poll = avahi_threaded_poll_get(_tpoll);

  int err;
  _client = avahi_client_new(poll, AVAHI_CLIENT_NO_FAIL, mdns::cbClient, this, &err);

  if (!_client) {
    error = avahi_strerror(err);
    return false;
  }

  if (avahi_threaded_poll_start(_tpoll) < 0) {
    error = "avahi threaded poll failed to start";
    return false;
  }

  return true;
}

void mDNS::update() {
  avahi_threaded_poll_lock(_tpoll); // lock the thread poll

  AvahiStringList *sl = avahi_string_list_new("autoUpdate=true", nullptr);

  const auto kvmap = _service->keyValList(AirPlayTCP);
  for (const auto &[key, val] : *kvmap) {
    sl = avahi_string_list_add_pair(sl, key, val);
  }

  constexpr auto iface = AVAHI_IF_UNSPEC;
  constexpr auto proto = AVAHI_PROTO_UNSPEC;
  constexpr auto pub_flags = (AvahiPublishFlags)0;

  const auto [reg_type, name] = _service->nameAndReg(AirPlayTCP);

  auto group = _groups.front(); // we're going to update the first (and only group)

  auto rc = avahi_entry_group_update_service_txt_strlst(group, iface, proto, pub_flags, name,
                                                        reg_type, NULL, sl);

  if (rc != 0) {
    fmt::print("mDNS::update(): failed rc={}\n", rc);
  }

  avahi_threaded_poll_unlock(_tpoll); // resume thread poll

  avahi_string_list_free(sl);
}

// void mDNS::serviceNameCollision(AvahiEntryGroup *group) {
//   char *n;

//   auto alt_service_name =
//   avahi_alternative_service_name(_service_name.c_str()); _service_name =
//   alt_service_name;

//   cerr << format("attempting to register alternate service[{}]",
//   _service_name);

//   /* And recreate the services */
//   AvahiClient *client = avahi_entry_group_get_client(group);
//   registerService(client);

//   // #debug(2, "avahi: service name collision, renaming service to '%s'",
//   service_name);
// }

} // namespace pierre