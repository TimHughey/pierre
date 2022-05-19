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

#include "mdns.hpp"
#include "core/service.hpp"
#include "mdns/callbacks.hpp"

#include <array>
#include <avahi-client/publish.h>
#include <fmt/format.h>
#include <list>
#include <memory>
#include <pthread.h>
#include <stdlib.h>

namespace pierre {

namespace shared {
std::optional<shmDNS> __mdns;
std::optional<shmDNS> &mdns() { return shared::__mdns; }
} // namespace shared

using enum service::Key;
using enum service::Type;

typedef std::vector<string> PreppedEntries;

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

    auto kvmap = Service::ptr()->keyValList(stype);

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
  const auto &[__unused_key, val_str] = Service::ptr()->fetch(apDeviceID);

  return string(val_str);
}

bool mDNS::groupAddService(AvahiEntryGroup *group, auto stype, const auto &prepped_entries) {
  constexpr auto iface = AVAHI_IF_UNSPEC;
  constexpr auto proto = AVAHI_PROTO_UNSPEC;
  constexpr auto pub_flags = (AvahiPublishFlags)0;

  const auto [reg_type, name] = Service::ptr()->nameAndReg(stype);

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

bool mDNS::resetGroupIfNeeded() {
  auto rc = true;

  // if there is already a group, reset it
  if (_groups.size()) {
    auto group = _groups.front();

    if (true) { // debug
      const auto state = avahi_entry_group_get_state(group);
      constexpr auto f = FMT_STRING("{} {} reset group count={} front={} state={}\n");
      fmt::print(f, runTicks(), fnName(), _groups.size(), fmt::ptr(group), state);
    }

    auto ac = avahi_entry_group_reset(group);

    if (ac != 0) {
      auto fmt_str = FMT_STRING("{} group reset failed={}\n");
      fmt::print(fmt_str, fnName(), ac);

      rc = false;
    }
  }

  return rc;
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
    constexpr auto f = FMT_STRING("{} BROWSER RESOLVE failed, service[{}] {} ");
    error = format(f, fnName(), name, mdns::error_string(client));
    return false;
  }

  return true;
}

void mDNS::saveGroup(AvahiEntryGroup *group) {
  if (_groups.size() > 0) {
    if (true) { // debug
      auto state = avahi_entry_group_get_state(group);

      constexpr auto f = FMT_STRING("{} {} group eixsts state={} same={}\n");
      fmt::print(f, runTicks(), fnName(), state, (_groups.front() == group));
    }

    return;
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
  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} groups size={}\n");
    fmt::print(f, runTicks(), fnName(), _groups.size());
  }

  avahi_threaded_poll_lock(_tpoll); // lock the thread poll

  AvahiStringList *sl = avahi_string_list_new("autoUpdate=true", nullptr);

  const auto kvmap = Service::ptr()->keyValList(AirPlayTCP);
  for (const auto &[key, val] : *kvmap) {
    if (false) { // debug
      constexpr auto f = FMT_STRING("{} {} {:>18}={}\n");
      fmt::print(f, runTicks(), fnName(), key, val);
    }

    sl = avahi_string_list_add_pair(sl, key, val);
  }

  constexpr auto iface = AVAHI_IF_UNSPEC;
  constexpr auto proto = AVAHI_PROTO_UNSPEC;
  constexpr auto pub_flags = (AvahiPublishFlags)0;

  const auto [reg_type, name] = Service::ptr()->nameAndReg(AirPlayTCP);

  auto group = _groups.front(); // we're going to update the first (and only group)

  auto rc = avahi_entry_group_update_service_txt_strlst(group, iface, proto, pub_flags, name,
                                                        reg_type, NULL, sl);

  if (rc != 0) {
    constexpr auto f = FMT_STRING("{} {} failed rc={}\n");
    fmt::print(f, runTicks(), fnName(), rc);
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