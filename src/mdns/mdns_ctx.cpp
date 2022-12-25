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

#include "mdns_ctx.hpp"
#include "config/config.hpp"

namespace pierre {
namespace mdns {

Ctx::Ctx() noexcept
    : all_for_now_state(false), //
      client_running{false}     //
{}

void Ctx::advertise(Service &service) noexcept {
  static constexpr csv fn_id{"ADVERTISE"};

  if (client && (entry_group.has_value() == false)) {
    // advertise the services
    // build and commit a single group containing two services:
    //  _airplay._tcp string list
    //  _raop._tcp string list

    Ctx *self = this;
    auto group = avahi_entry_group_new(client, cb_entry_group, self);

    for (const auto stype : std::array{txt_type::AirPlayTCP, txt_type::RaopTCP}) {
      constexpr AvahiPublishFlags flags = static_cast<AvahiPublishFlags>(0);
      constexpr ccs DEFAULT_HOST{nullptr};
      constexpr ccs DEFAULT_DOMAIN{nullptr};

      const auto [reg_type, name] = service.name_and_reg(stype);

      std::vector<ccs> ccs_ptrs;
      auto entries = service.make_txt_entries(stype); // must remain in scope

      for (const auto &entry : entries) {
        ccs_ptrs.emplace_back(entry.c_str());
      }

      auto cfg = Config::ptr();

      auto sl = avahi_string_list_new_from_array(ccs_ptrs.data(), ccs_ptrs.size());
      auto rc = avahi_entry_group_add_service_strlst(
          entry_group.value(), AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, flags, name.data(),
          reg_type.data(), DEFAULT_HOST, DEFAULT_DOMAIN, cfg->at("mdns.port"sv).value_or(7000), sl);

      if (rc == AVAHI_ERR_COLLISION) {
        INFO(module_id, fn_id, "AirPlay2 name in use, name={}\n", name);
        avahi_string_list_free(sl);
      } else if (rc != AVAHI_OK) {
        INFO(module_id, fn_id, "unhandled error={}\n", avahi_strerror(rc));
      }
    }

    if (auto rc = avahi_entry_group_commit(group); rc != AVAHI_OK) {
      INFO(module_id, fn_id, "unhandled error={}\n", avahi_strerror(rc));
    }
  }
}

void Ctx::browse(csv stype) noexcept {
  auto *self = this;

  auto sb = avahi_service_browser_new(client,              // client
                                      AVAHI_IF_UNSPEC,     // network interface
                                      AVAHI_PROTO_UNSPEC,  // any protocol
                                      stype.data(),        // service type
                                      nullptr,             // domain
                                      (AvahiLookupFlags)0, // lookup flags
                                      Ctx::cb_browse,      // callback
                                      self);               // userdata
  if (sb == nullptr) {
    INFO(module_id, "BROWSE", "create failed reason={}\n", error_string(client));
  } else {
    INFOX(module_id, "BROWSE", "initiated browse for stype={}\n", stype);
  }
}

void Ctx::cb_client(AvahiClient *client, AvahiClientState state, void *user_data) {
  auto ctx = static_cast<Ctx *>(user_data);

  INFO(module_id, "CB_CLIENT", "client={} user_data={} ctx={}\n", fmt::ptr(client),
       fmt::ptr(user_data), fmt::ptr(ctx));

  // name the avahi thread (if needed)
  uint8v buff(32, 0x00);
  auto tid = pthread_self();
  pthread_getname_np(tid, buff.data_as<char>(), buff.size());

  if (buff.view() != thread_name) {
    pthread_setname_np(tid, thread_name.data());
  }

  // always perfer the client passed to the callback
  ctx->client = client;

  switch (state) {
  case AVAHI_CLIENT_CONNECTING: {
    INFO(module_id, "CONNECTING", "thread={:#x}\n", tid);
  } break;

  case AVAHI_CLIENT_S_REGISTERING: {
    // if there is already a group, reset it
    if (ctx->entry_group.has_value()) {
      auto group = ctx->entry_group.value();

      const auto state = avahi_entry_group_get_state(group);
      INFO(module_id, "GROUP_RESET", "group={} state={}\n", fmt::ptr(group), state);

      auto ac = avahi_entry_group_reset(group);
      INFO(module_id, "GROUP_RESET", "avahi_strerror={}\n", avahi_strerror(ac));
    }

  } break;

  case AVAHI_CLIENT_S_RUNNING: {
    ctx->domain = avahi_client_get_domain_name(client);
    ctx->client_running.store(true);
    ctx->client_running.notify_all();
    // ctx->advertise(mdns->service());
  } break;

  case AVAHI_CLIENT_FAILURE: {
    INFO(module_id, "CLIENT", "FAILED, reason={}\n", ctx->error_string(client));
  } break;

  case AVAHI_CLIENT_S_COLLISION: {
    INFO(module_id, "CLIENT", "name collison, reason={}\n", ctx->error_string(client));
  } break;
  }
}

// called when a service becomes available or is removed from the network
void Ctx::cb_browse(AvahiServiceBrowser *b, AvahiIfIndex iface, AvahiProtocol protocol,
                    AvahiBrowserEvent event, ccs name, ccs type, ccs domain,
                    [[maybe_unused]] AvahiLookupResultFlags flags,
                    void *user_data) { // static

  static constexpr csv fn_id{"CB_BROWSE"};

  auto ctx = static_cast<Ctx *>(user_data);

  switch (event) {
  case AVAHI_BROWSER_FAILURE: {
    INFO(module_id, "BROWSE", "browser={} error={}\n", fmt::ptr(b), ctx->error_string(b));
    avahi_threaded_poll_quit(ctx->tpoll);
  } break;

  case AVAHI_BROWSER_NEW: {
    INFOX(module_id, fn_id, "NEW host={} type={} domain={}\n", name, type, domain);

    // default flags (for clarity)
    AvahiLookupFlags flags = static_cast<AvahiLookupFlags>(0);

    auto r = avahi_service_resolver_new(ctx->client,        // the client
                                        iface,              // same interface
                                        protocol,           // same protocol
                                        name,               // same service name
                                        type,               // same service type
                                        domain,             // same domain
                                        AVAHI_PROTO_UNSPEC, // use any protocol
                                        flags,              // resolve flags
                                        cb_resolve,         // callback when resolved
                                        user_data);         // same userdata (ctx)

    // ignore the returned resolver object. the callback function will free it.
    // if the server is terminated before the callback function is called the server
    // will free the resolver for us.

    if (!r) INFO(module_id, fn_id, "RESOLVER failed, service={}\n", type);

  } break;

  case AVAHI_BROWSER_REMOVE: {
    ctx->browse_remove(name);
  } break;

  case AVAHI_BROWSER_ALL_FOR_NOW:
  case AVAHI_BROWSER_CACHE_EXHAUSTED: {
    ctx->all_for_now(true);
  } break;
  }
}

void Ctx::cb_entry_group(AvahiEntryGroup *group, AvahiEntryGroupState state, void *user_data) {
  auto ctx = static_cast<mdns::Ctx *>(user_data);
  auto &entry_group = ctx->entry_group;

  INFO(module_id, "CB_EB", "group={} state={} user_data={}\n",
       fmt::ptr(entry_group.value_or(nullptr)), state, fmt::ptr(user_data));

  ctx->entry_group_state = state;

  switch (state) {
  case AVAHI_ENTRY_GROUP_ESTABLISHED: {
    if (ctx->entry_group.has_value()) {
      const auto state = avahi_entry_group_get_state(group);
      INFO(module_id, "GROUP_ESTAB", "exists state={} same={}\n", state, (*entry_group == group));
      return;
    }
  } break;

  // do nothing special for these states
  case AVAHI_ENTRY_GROUP_COLLISION: {
    INFO(module_id, "COLLISION", "group={}\n", fmt::ptr(entry_group.value_or(nullptr)));
  } break;

  case AVAHI_ENTRY_GROUP_FAILURE: {

    INFO(module_id, "COLLISION", "group={}\n", fmt::ptr(entry_group.value_or(nullptr)));
  } break;

  case AVAHI_ENTRY_GROUP_UNCOMMITED: {
    // new entry group created, save it
    entry_group.emplace(group);

  } break;

  case AVAHI_ENTRY_GROUP_REGISTERING: {
    INFO(module_id, "REGISTER", "REGISTERING group={}\n", fmt::ptr(entry_group.value_or(nullptr)));
  } break;
  }
}

void Ctx::cb_resolve(AvahiServiceResolver *r, AvahiIfIndex, AvahiProtocol protocol,
                     AvahiResolverEvent event, ccs name, ccs type, ccs domain, ccs host_name,
                     const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
                     AvahiLookupResultFlags, void *user_data) {

  auto ctx = static_cast<Ctx *>(user_data);

  switch (event) {
  case AVAHI_RESOLVER_FAILURE: {
    if (avahi_client_errno(ctx->client) == AVAHI_ERR_TIMEOUT) {
      ctx->browse_remove(name);
    }

    INFO(module_id, "RESOLVE", "FAILED, reason={}\n", ctx->error_string(ctx->client));
    break;
  }

  case AVAHI_RESOLVER_FOUND: {
    std::array<char, AVAHI_ADDRESS_STR_MAX> addr_str{0};

    ctx->resolved({
        .domain = domain,
        .hostname = host_name,                                                       //
        .name_net = name,                                                            //
        .address = avahi_address_snprint(addr_str.data(), addr_str.size(), address), //
        .type = type,                                                                //
        .port = port,                                                                //
        .protocol = avahi_proto_to_string(protocol),                                 //
        .txt_list = make_txt_list(txt)                                               //
    });
  }

  break;
  }

  if (r) avahi_service_resolver_free(r);
}

const string Ctx::init(Service &service) noexcept {
  string msg;

  tpoll = avahi_threaded_poll_new();
  auto poll = avahi_threaded_poll_get(tpoll);

  auto self = this;
  int err{0};
  client = avahi_client_new(poll, AVAHI_CLIENT_NO_FAIL, Ctx::cb_client, self, &err);

  if (client && !err) {
    err = avahi_threaded_poll_start(tpoll);

    client_running.wait(false);
    advertise(service);
    INFO(module_id, "INIT", "client running, sizeof={}\n", sizeof(Ctx));

    if (!err) {
      string stype = config()->table().at_path("mdns.service"sv).value_or<string>("_ruth._tcp");
      browse(stype);
    } else {
      msg = string(avahi_strerror(err));
    }
  }

  return msg;
}

zc::TxtList Ctx::make_txt_list(AvahiStringList *txt) noexcept { // static
  zc::TxtList txt_list;

  if (txt != nullptr) {
    for (AvahiStringList *e = txt; e != nullptr; e = avahi_string_list_get_next(e)) {
      char *key;
      char *val;
      size_t val_size;

      if (auto rc = avahi_string_list_get_pair(e, &key, &val, &val_size); rc == 0) {
        txt_list.emplace_back(key, val);

        avahi_free(key);
        avahi_free(val);
      };
    }
  }

  return txt_list;
}

void Ctx::resolved(const ZeroConf::Details zcd) noexcept {
  auto [zc_it, inserted] = zcs_map.try_emplace(zcd.name_net, ZeroConf(zcd));
  const auto &zc = zc_it->second;

  INFOX(module_id, "RESOLVED", "{} {}\n", inserted ? "resolved" : "already know", zc.inspect());

  if (auto it = zcs_proms.find(zc.name_short()); it != zcs_proms.end()) {
    it->second.set_value(zc);
    zcs_proms.erase(it);
  }
}

void Ctx::update(Service &service) noexcept {

  const auto entries = service.key_val_for_type(txt_type::AirPlayTCP);
  const auto [type, name] = service.name_and_reg(txt_type::AirPlayTCP);

  // update() is called outside the event loop so we must lock the thread poll
  lock();

  // include autoUpdate as a signal to avahi this is an update
  AvahiStringList *sl = avahi_string_list_new("autoUpdate=true", nullptr);

  // build the txt avahi string list
  for (const auto &[key, val] : entries) {
    sl = avahi_string_list_add_pair(sl, key.data(), val.data());
  }

  AvahiPublishFlags flags = static_cast<AvahiPublishFlags>(0);

  auto err = avahi_entry_group_update_service_txt_strlst( // update the group
      entry_group.value(),                                // avahi group to update
      AVAHI_IF_UNSPEC,                                    // any interface
      AVAHI_PROTO_UNSPEC,                                 // any protocol
      flags,                                              // publish flags
      name.data(),                                        // name to update
      type.data(),                                        // service type to update
      nullptr,                                            // domain (default)
      sl                                                  // string list to apply to the group
  );

  if (err) INFO(module_id, "UPDATE", "FAILED, reason={}\n", avahi_strerror(err));

  unlock();                   // resume thread poll
  avahi_string_list_free(sl); // clean up the string list, avahi copied it
}

} // namespace mdns
} // namespace pierre