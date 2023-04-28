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
#include "base/logger.hpp"

namespace pierre {
namespace mdns {

AvahiThreadedPoll *Ctx::tpoll{nullptr};
AvahiClient *Ctx::client{nullptr};

Ctx::Ctx(const string &stype, Service *service, Port receiver_port) noexcept
    : stype(stype),                 //
      service(service),             //
      receiver_port(receiver_port), //
      all_for_now_state(false),     //
      client_running{false},        //
      threaded_poll_quit{false}     //
{
  // CONSTRUCTOR DESIGN NOTES:
  //
  //  -allocates and starts the avahi threaded poll functionality
  //  -invoked before Logger is setup

  if ((tpoll == nullptr) && (client == nullptr)) {
    tpoll = avahi_threaded_poll_new();

    if (tpoll == nullptr) {
      err_msg.assign("failed to allocate threaded_poll");
      return;
    }

    auto poll = avahi_threaded_poll_get(tpoll);
    int err{0};

    // notes:
    //  1. we don't capture the client pointer here. rather, we wait until the callback
    //     fires.  this is so we can detect the first invocation of the callback and
    //     set the thread name.
    //  2. we pass AVAHI_CLIENT_NO_FAIL to ensure the client is created even if the
    //     daemon is unavailable and to enter CLIENT_FAILURE state if the client
    //     is forced to disconnect
    const AvahiClientFlags flags = AVAHI_CLIENT_NO_FAIL;
    auto new_client = avahi_client_new(poll, flags, Ctx::cb_client, this, &err);

    // confirm allocation and no errors
    if (new_client == nullptr) {
      err_msg.assign("failed to allocate client");
    } else if (err) {
      err_msg.assign(avahi_strerror(err));
      return;
    }

    // start the threaded poll, if no error the advertise and browse
    if (err = avahi_threaded_poll_start(tpoll); err) {
      err_msg.assign(avahi_strerror(err));
    }
  } else {
    err_msg = fmt::format("warning: tpoll already created, tpoll={}\n", fmt::ptr(tpoll));
  }
}

void Ctx::run(string &msg) noexcept {

  auto w = std::back_inserter(msg);

  if (err_msg.empty()) {
    advertise();
    browse(stype);

    msg = fmt::format("mdns::Ctx sizeof={:>5} service={}", sizeof(Ctx), stype);

  } else {
    // constructor failed, return the error msg
    fmt::format_to(w, "{}", err_msg);
  }
}

Ctx::~Ctx() noexcept {

  client_running = false;

  if (entry_group.has_value()) {
    lock();
    avahi_entry_group_reset(entry_group.value());
    unlock();
  }

  threaded_poll_quit.wait(false);

  avahi_client_free(client);

  avahi_threaded_poll_stop(tpoll);
  avahi_threaded_poll_free(tpoll);
}

void Ctx::advertise() noexcept {
  static constexpr csv fn_id{"advertise"};

  if (client_running.load() != true) {
    INFO_AUTO("waiting for client to start\n");
    client_running.wait(false);
  }

  INFO_AUTO("client_running={}\n", client_running.load());

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

      const auto [reg_type, name] = service->name_and_reg(stype);

      std::vector<ccs> ccs_ptrs;
      auto entries = service->make_txt_entries(stype); // must remain in scope

      for (const auto &entry : entries) {
        ccs_ptrs.emplace_back(entry.c_str());
      }

      auto sl = avahi_string_list_new_from_array(ccs_ptrs.data(), ccs_ptrs.size());
      auto rc = avahi_entry_group_add_service_strlst(
          entry_group.value(), AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, flags, name.data(),
          reg_type.data(), DEFAULT_HOST, DEFAULT_DOMAIN, receiver_port, sl);

      if (rc == AVAHI_ERR_COLLISION) {
        INFO_AUTO("AirPlay2 name in use, name={}\n", name);
        avahi_string_list_free(sl);
      } else if (rc != AVAHI_OK) {
        INFO_AUTO("unhandled error={}\n", avahi_strerror(rc));
      }
    }

    if (auto rc = avahi_entry_group_commit(group); rc != AVAHI_OK) {
      INFO_AUTO("unhandled error={}\n", avahi_strerror(rc));
    }
  }
}

void Ctx::all_for_now(bool next_val) noexcept {
  constexpr auto fn_id{"all_for_now"sv};

  auto prev = std::atomic_exchange(&all_for_now_state, next_val);

  INFO_AUTO("previous state={}\n", prev);
}

void Ctx::browse(csv stype) noexcept {
  static constexpr csv fn_id{"browse"};
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
    INFO_AUTO("create failed {} reason={}\n", stype, error_string(client));
  } else {
    INFO_AUTO("initiated browse for {}\n", stype);
  }
}

void Ctx::browse_remove(const string name) noexcept {
  // NOTE: this is called from an avahi callback while the ctx is locked
  //       and is therefore protected from race conditions with zservice()

  // first, clean up the promise map by setting the promise value to
  // the default ZeroConf so the promise holder can take action
  if (auto it = zcs_proms.find(name); it != zcs_proms.end()) {
    it->second.set_value(ZeroConf());
    zcs_proms.erase(it);
  }

  // now, remove from
  if (auto it = zcs_map.find(name); it != zcs_map.end()) {
    zcs_map.erase(it);
  }
}

void Ctx::cb_client(AvahiClient *client, AvahiClientState state, void *user_data) {
  static constexpr csv fn_id{"cb_client"};

  auto ctx = static_cast<Ctx *>(user_data);

  // is this the first invocation of the callback?
  if (client && (ctx->client == nullptr)) {

    INFO_AUTO("STORING, client={} ctx={}\n", fmt::ptr(client), fmt::ptr(ctx));

    ctx->client = client; // save the client (for comparison later)

  } else if (client != ctx->client) {
    INFO_AUTO("CLIENT MISMATCH, {}/{} \n", fmt::ptr(client), fmt::ptr(ctx->client));
    return;
  }

  switch (state) {
  case AVAHI_CLIENT_CONNECTING: {
    INFO_AUTO("CONNECTING, client={}\n", fmt::ptr(ctx->client));
  } break;

  case AVAHI_CLIENT_S_REGISTERING: {
    INFO_AUTO("REGISTERING, client={}\n", fmt::ptr(ctx->client));

    // if there is already a group, reset it
    if (ctx->entry_group.has_value()) {
      auto group = ctx->entry_group.value();
      ctx->entry_group.reset();

      const auto state = avahi_entry_group_get_state(*ctx->entry_group);

      auto ac = avahi_entry_group_reset(group);
      INFO_AUTO("GROUP RESET, group={} state={} avahi_strerror={}\n", fmt::ptr(group), state,
                avahi_strerror(ac));

      avahi_entry_group_free(group);
    }

  } break;

  case AVAHI_CLIENT_S_RUNNING: {
    constexpr auto tname{"pierre_mdns"};
    pthread_setname_np(pthread_self(), tname);

    ctx->domain = avahi_client_get_domain_name(client);
    string vsn(avahi_client_get_version_string(client));

    INFO_AUTO("RUNNING, vsn='{}' domain={}\n", vsn, ctx->domain);

    if (std::atomic_exchange(&(ctx->client_running), true) == false) {
      ctx->client_running.notify_all();
    }

  } break;

  case AVAHI_CLIENT_FAILURE: {
    INFO_AUTO("FAILED, reason={}\n", error_string(client));
    avahi_threaded_poll_quit(ctx->tpoll);
  } break;

  case AVAHI_CLIENT_S_COLLISION: {
    INFO_AUTO("NAME COLLISION, reason={}\n", error_string(client));
  } break;
  }
}

// called when a service becomes available or is removed from the network
void Ctx::cb_browse(AvahiServiceBrowser *b, AvahiIfIndex iface, AvahiProtocol protocol,
                    AvahiBrowserEvent event, ccs name, ccs type, ccs domain, AvahiLookupResultFlags,
                    void *user_data) { // static

  constexpr auto fn_id{"cb_browse"sv};

  auto ctx = static_cast<Ctx *>(user_data);

  switch (event) {
  case AVAHI_BROWSER_FAILURE: {
    INFO_AUTO("browser={} error={}\n", fmt::ptr(b), error_string(b));
    avahi_threaded_poll_quit(ctx->tpoll);
  } break;

  case AVAHI_BROWSER_NEW: {
    INFO_AUTO("NEW {} {}\n", type, name);

    // default flags (for clarity)
    AvahiLookupFlags flags{};

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

    if (!r) INFO_AUTO("RESOLVER failed, service={}\n", type);

  } break;

  case AVAHI_BROWSER_REMOVE: {
    INFO_AUTO("REMOVE {} {} {}\n", name, type, domain);
    ctx->browse_remove(name);
  } break;

  case AVAHI_BROWSER_ALL_FOR_NOW:
  case AVAHI_BROWSER_CACHE_EXHAUSTED: {
    ctx->all_for_now(true);
  } break;
  }
}

void Ctx::cb_entry_group(AvahiEntryGroup *group, AvahiEntryGroupState state, void *user_data) {
  static constexpr csv fn_id{"cb_evt_grp"};

  auto ctx = static_cast<mdns::Ctx *>(user_data);
  auto &entry_group = ctx->entry_group;

  ctx->entry_group_state = state;

  switch (state) {
  case AVAHI_ENTRY_GROUP_ESTABLISHED: {
    INFO_AUTO("ESTABLISHED, group={}\n", fmt::ptr(entry_group.value_or(nullptr)));
  } break;

  // do nothing special for these states
  case AVAHI_ENTRY_GROUP_COLLISION: {
    INFO_AUTO("COLLISION, group={}\n", fmt::ptr(group));
  } break;

  case AVAHI_ENTRY_GROUP_FAILURE: {
    INFO_AUTO("FAILURE, group={}\n", fmt::ptr(group));
  } break;

  case AVAHI_ENTRY_GROUP_UNCOMMITED: {
    if (ctx->client_running == true) {

      INFO_AUTO("UNCOMMITTED, STORING group={}\n", fmt::ptr(group));
      // new entry group created, save it
      entry_group.emplace(group);
    } else {
      INFO_AUTO("UNCOMMITTED, client shutting down\n");
      avahi_threaded_poll_quit(ctx->tpoll);
      ctx->threaded_poll_quit = true;
      ctx->threaded_poll_quit.notify_all();
    }

  } break;

  case AVAHI_ENTRY_GROUP_REGISTERING: {
    INFO_AUTO("REGISTERING, group={}\n", fmt::ptr(entry_group.value_or(nullptr)));
  } break;
  }
}

void Ctx::cb_resolve(AvahiServiceResolver *r, AvahiIfIndex, AvahiProtocol protocol,
                     AvahiResolverEvent event, ccs name, ccs type, ccs domain, ccs host_name,
                     const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
                     AvahiLookupResultFlags, void *user_data) {
  static constexpr csv fn_id{"resolve"};
  auto ctx = static_cast<Ctx *>(user_data);

  switch (event) {
  case AVAHI_RESOLVER_FAILURE: {
    if (avahi_client_errno(ctx->client) == AVAHI_ERR_TIMEOUT) {
      INFO_AUTO("failed for {}\n", name);
      ctx->browse_remove(name);
    }

    INFO_AUTO("FAILED, reason={}\n", error_string(ctx->client));
  } break;

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
  } break;
  }

  if (r) avahi_service_resolver_free(r);
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
  static constexpr csv fn_id{"resolved"};
  auto [zc_it, inserted] = zcs_map.try_emplace(zcd.name_net, ZeroConf(zcd));
  const auto &zc = zc_it->second;

  INFO_AUTO("{} {}\n", inserted ? "resolved" : "already know", zc.inspect());

  if (auto it = zcs_proms.find(zc.name_short()); it != zcs_proms.end()) {
    it->second.set_value(zc);
    zcs_proms.erase(it);
  }
}

void Ctx::update() noexcept {
  static constexpr csv fn_id{"update"};
  const auto entries = service->key_val_for_type(txt_type::AirPlayTCP);
  const auto [type, name] = service->name_and_reg(txt_type::AirPlayTCP);

  // update() is called outside the event loop so we must lock the thread poll
  lock();

  // include autoUpdate as a signal to avahi this is an update
  AvahiStringList *sl = avahi_string_list_new("autoUpdate=true", nullptr);

  // build the txt avahi string list
  for (const auto &[key, val] : entries) {
    sl = avahi_string_list_add_pair(sl, key.data(), val.data());
  }

  AvahiPublishFlags flags{};

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

  if (err) INFO_AUTO("FAILED, reason={}\n", avahi_strerror(err));

  unlock();                   // resume thread poll
  avahi_string_list_free(sl); // clean up the string list, avahi copied it
}

ZeroConfFut Ctx::zservice(csv name) noexcept {
  static constexpr csv fn_id{"zservice"};

  ZeroConfProm prom;
  ZeroConfFut fut{prom.get_future()}; // get the

  lock(); // lock the context to prevent changes while searching

  auto it = std::find_if(zcs_map.begin(), zcs_map.end(),
                         [&name](auto it) { return it.second.match_name(name); });

  if (it != zcs_map.end()) {
    INFO_AUTO("{} known, setting promise\n", name);
    // the name is already resolved, we're done... immediately set the promise
    prom.set_value(it->second);
  } else {
    // this name isn't yet resolved, save the promise for when it is
    auto inserted{false};

    do {
      // if there is an existing promise for this name it's value is
      // set to a default ZeroConf and the new promise is stored
      auto try_rc = zcs_proms.try_emplace(name.data(), std::move(prom));

      inserted = try_rc.second;

      if (inserted == false) {
        INFO_AUTO("found pending promise for {}, clearing it\n", name);
        auto &existing_prom = try_rc.first->second;
        existing_prom.set_value(ZeroConf());
      }

    } while (!inserted);
  }

  unlock();

  return fut;
}

} // namespace mdns
} // namespace pierre