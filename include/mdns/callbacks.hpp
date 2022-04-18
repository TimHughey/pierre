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

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>

#include <avahi-client/lookup.h>
#include <avahi-common/alternative.h>

#include "mdns.hpp"

namespace pierre {
namespace mdns {

using namespace std;

extern void cbClient(AvahiClient *client, AvahiClientState state, void *userdata);
extern void cbBrowse(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol,
                     AvahiBrowserEvent event, const char *name, const char *type,
                     const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
                     void *userdata);

extern void cbBrowse(AvahiClient *c, AvahiClientState state, void *userdata);

extern void cbEntryGroup(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata);

extern void cbResolve(AvahiServiceResolver *r, [[maybe_unused]] AvahiIfIndex interface,
                      [[maybe_unused]] AvahiProtocol protocol, AvahiResolverEvent event,
                      const char *name, const char *type, const char *domain,
                      [[maybe_unused]] const char *host_name,
                      [[maybe_unused]] const AvahiAddress *address, uint16_t port,
                      [[maybe_unused]] AvahiStringList *txt,
                      [[maybe_unused]] AvahiLookupResultFlags flags, void *userdata);

extern string error_string(AvahiClient *client);
extern string error_string(AvahiEntryGroup *group);
extern string error_string(AvahiServiceBrowser *browser);

} // namespace mdns

} // namespace pierre