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
#include <iostream>
#include <memory>
#include <pthread.h>
#include <sodium.h>
#include <stdlib.h>

#include "mdns.hpp"
#include "pair/pair.h"

namespace pierre {

using namespace std;
using namespace fmt;

mDNS::mDNS(ConfigPtr cfg) : _cfg{cfg} {
  constexpr auto fmt_str = "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}";
  const auto addr = _cfg->hw_addr;

  const auto id = format(fmt_str, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  _service_name = format("{}@Jophiel", id);
}

bool mDNS::start() {

  if (sodium_init() < 0) {
    return false;
  }

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

string mDNS::error_string(AvahiClient *client) {
  auto client_errno = avahi_client_errno(client);
  return string(avahi_strerror(client_errno));
}

string mDNS::error_string(AvahiEntryGroup *group) {
  // cerr << "DEBUG " << __PRETTY_FUNCTION__ << endl;
  // auto client = avahi_entry_group_get_client(group);
  // auto msg = error_string(client);
  return format("Entry Group failure");
}

string mDNS::error_string(AvahiServiceBrowser *browser) {
  auto client = avahi_service_browser_get_client(browser);
  auto client_errno = avahi_client_errno(client);
  auto msg = avahi_strerror(client_errno);
  return format("browser failure: {}", msg);
}

AvahiStringList *mDNS::makeBonjour() {
  AvahiStringList *slist;

  constexpr auto mask32 = 0xffffffff;
  uint64_t features_hi = (_cfg->airplay_features >> 32) & mask32;
  uint64_t features_lo = (_cfg->airplay_features & mask32);

  array<uint8_t, 32> public_key{0};
  pair_public_key_get(PAIR_SERVER_HOMEKIT, public_key.data(), _cfg->airplay_device_id.c_str());

  auto pk_buffer = fmt::memory_buffer();

  for (const auto &i : public_key) {
    fmt::format_to(std::back_inserter(pk_buffer), "{:02x}", i);
  }

  auto ft = fmt::format("ft={:#02X},{:#02X}", features_lo, features_hi);
  auto fv = fmt::format("fv={}", _cfg->firmware_version);
  auto pk = fmt::format("pk={}", string(pk_buffer.data(), pk_buffer.size()));
  // auto di = fmt::format("device_id={}", _cfg->airplay_device_id);
  // auto fl = fmt::format("flags={}", _cfg->airplay_statusflags);
  // auto pi = fmt::format("pi={}", _cfg->airplay_pi.data());
  // auto gi = fmt::format("gid={}", _cfg->airplay_pi.data()); // NOTE: initial, changes after
  // connection

  std::array common{"cn=0,1",  "da=true", "et=0,3,5", ft.c_str(), fv.c_str(), "am=Lights by Pierre",
                    "sf=0x04", "tp=UDP",  "vn=65537", "vs=366.0", pk.c_str()};

  return avahi_string_list_new_from_array(common.data(), common.size());
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

bool mDNS::registerService(AvahiClient *use_client) {
  AvahiClient *client = use_client;

  if (use_client == nullptr) {
    client = _client;
  }

  if (client == nullptr) {
    cerr << "client=nullptr" << endl;
  }

  if (_group != nullptr) {
    cerr << "Entry group already created" << endl;
    return false;
  }

  _group = avahi_entry_group_new(client, cbEntryGroup, this);

  auto string_list = makeBonjour();

  auto rc = avahi_entry_group_add_service_strlst(
      _group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, (AvahiPublishFlags)0, _service_name.c_str(),
      regType.c_str(), NULL, NULL, _cfg->port, string_list);

  if (rc == AVAHI_ERR_COLLISION) {
    cerr << format("AirPlay 2 name [{}] already in use", _service_name) << endl;
    return false;
  }

  rc = avahi_entry_group_commit(_group);

  return (rc == AVAHI_OK) ? true : false;
}

void mDNS::serviceNameCollision(AvahiEntryGroup *group) {
  char *n;

  auto alt_service_name = avahi_alternative_service_name(_service_name.c_str());
  _service_name = alt_service_name;

  cerr << format("attempting to register alternate service[{}]", _service_name);

  /* And recreate the services */
  AvahiClient *client = avahi_entry_group_get_client(group);
  registerService(client);

  // #debug(2, "avahi: service name collision, renaming service to '%s'", service_name);
}

} // namespace pierre