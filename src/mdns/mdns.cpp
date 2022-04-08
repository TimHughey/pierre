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
#include <fmt/format.h>
#include <gcrypt.h>
#include <iostream>
#include <list>
#include <memory>
#include <pthread.h>
#include <sodium.h>
#include <stdlib.h>

#include "mdns.hpp"
#include "mdns/service.hpp"
#include "pair/pair.h"

namespace pierre {

using namespace std;
using namespace mdns;
using namespace fmt;

typedef std::vector<string> PreppedEntries;

mDNS::mDNS(const Opts &opts) : _host(opts.host) {
  _service_base = opts.service_base;
  _firmware_vsn = opts.firmware_vsn;
}

void mDNS::advertise(AvahiClient *client) {
  _client = client;

  if (_client == nullptr) {
    fmt::print("mDNS advertise(): client={}\n", fmt::ptr(client));
    return;
  }

  if (_groups.size() == 1) {
    fmt::print("mDNS advertise(): services already advertised: ");

    for (const auto &service : _services) {
      fmt::print("{} ", service.name());
    }

    fmt::print("\n");

    return;
  }

  // advertise the services
  // build and commit a single group containing two services:
  //  _airplay._tcp string list
  //  _raop._tcp string list

  auto group = avahi_entry_group_new(_client, cbEntryGroup, this);

  for (const auto &service : _services) {
    PreppedEntries entries;
    const auto &sl_map = service.stringListMap();

    for (const auto &[key, val] : sl_map) {
      fmt::basic_memory_buffer<char, 128> sl_entry;
      ;

      fmt::format_to(sl_entry, "{}={}", key, val);
      entries.emplace_back(fmt::to_string(sl_entry));
    }

    groupAddService(group, service, entries);
  }

  auto rc = avahi_entry_group_commit(group);

  if (rc != AVAHI_OK) {
    fmt::print("advertise(): Unhandled error={}\n", avahi_strerror(rc));
  }
}

bool mDNS::start() {
  if (sodium_init() < 0) {
    return false;
  }

  gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
  gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

  makePrivateKey();
  makeServices();

  _tpoll = avahi_threaded_poll_new();
  auto poll = avahi_threaded_poll_get(_tpoll);

  int err;
  _client = avahi_client_new(poll, AVAHI_CLIENT_NO_FAIL, cbClient, this, &err);

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

bool mDNS::groupAddService(AvahiEntryGroup *group, const auto &service,
                           const auto &prepped_entries) {
  constexpr auto iface = AVAHI_IF_UNSPEC;
  constexpr auto proto = AVAHI_PROTO_UNSPEC;
  constexpr auto pub_flags = (AvahiPublishFlags)0;

  const auto name = service.name();
  const auto reg_type = service.regType();

  std::vector<const char *> string_pointers;
  for (auto i = 0; auto &entry : prepped_entries) {
    string_pointers.emplace_back(entry.c_str());
  }

  auto sl = avahi_string_list_new_from_array(string_pointers.data(), string_pointers.size());
  auto rc = avahi_entry_group_add_service_strlst(group, iface, proto, pub_flags, name, reg_type,
                                                 NULL, NULL, _port, sl);

  if (rc == AVAHI_ERR_COLLISION) {
    fmt::print("AirPlay2 name [{}] already in use\n");
    avahi_string_list_free(sl);
    return false;
  }

  if (rc != AVAHI_OK) {
    fmt::print("groupAddService(): Unhandled error={}\n", avahi_strerror(rc));
  }

  return (rc == AVAHI_OK) ? true : false;
}

void mDNS::makeServices() {
  const auto base_data = Service::BaseData{
      .host = _host, .service = _service_base, .pk = _pk, .firmware_vsn = _firmware_vsn};

  // store static base data necessary to create services
  Service::setBaseData(base_data);

  auto airplay = AirPlay();
  auto raop = Raop();

  airplay.build();
  _services.emplace_back(airplay);

  raop.build();
  _services.emplace_back(raop);
}

void mDNS::makePrivateKey() {
  auto *dest = _pk_bytes.data();
  auto *secret = _host->hwAddr().c_str();

  pair_public_key_get(PAIR_SERVER_HOMEKIT, dest, secret);

  _pk = fmt::format("{:02x}", fmt::join(_pk_bytes, ""));
}

bool mDNS::resolverNew(AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol,
                       const char *name, const char *type, const char *domain,
                       AvahiProtocol aprotocol, AvahiLookupFlags flags,
                       AvahiServiceResolverCallback callback, void *userdata) {
  _resolver =
      avahi_service_resolver_new(client, interface, protocol, name, type, domain,
                                 AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)9, cbResolve, userdata);

  if (_resolver == nullptr) {
    error = format("BROWSER RESOLVE failed, service[{}] {} ", name, error_string(client));
    return false;
  }

  return true;
}

// void mDNS::serviceNameCollision(AvahiEntryGroup *group) {
//   char *n;

//   auto alt_service_name = avahi_alternative_service_name(_service_name.c_str());
//   _service_name = alt_service_name;

//   cerr << format("attempting to register alternate service[{}]", _service_name);

//   /* And recreate the services */
//   AvahiClient *client = avahi_entry_group_get_client(group);
//   registerService(client);

//   // #debug(2, "avahi: service name collision, renaming service to '%s'", service_name);
// }

} // namespace pierre