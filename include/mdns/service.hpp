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

#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "mdns/status_flags.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>

namespace pierre {

// service type (key into service_txt_map)
enum txt_type : int8_t { AirPlayTCP = 0, RaopTCP };

// available service txt options (key into lookup map)
enum txt_opt : int8_t {
  AirPlayRegNameType,
  apAccessControlLevel,
  apAirPlayPairingIdentity,
  apAirPlayVsn,
  apDeviceID,
  apFeatures,
  apGroupDiscoverableLeader,
  apGroupUUID,
  apManufacturer,
  apModel,
  apProtocolVsn,
  apRequiredSenderFeatures,
  apSerialNumber,
  apStatusFlags,
  apSystemFlags,
  FirmwareVsn,
  mdAirPlayVsn,
  mdAirTunesProtocolVsn,
  mdCompressionTypes,
  mdDigestAuthKey,
  mdEncryptTypes,
  mdFeatures,
  mdMetadataTypes,
  mdModel,
  mdSystemFlags,
  mdTransportTypes,
  plFeatures,
  PublicKey,
  RaopRegNameType,
  ServiceName,
};

// data structures representing the TXT option key/val:
using txt_key_t = string;                         // txt key
using txt_val_t = string;                         // txt val
using txt_kv_t = std::pair<txt_key_t, txt_val_t>; // combined option kv
using lookup_map_t = std::map<txt_opt, txt_kv_t>; // map of enum TXT names to kv
using txt_opt_seq_t = std::vector<txt_opt>;       // TXT string order of kv

struct service_def {
  txt_type type;
  txt_opt_seq_t order;
};

using service_txt_map = std::map<txt_type, service_def>;

class Service {

public:
  Service() = default;

  // calculate the runtime values (called once at start up)
  void init() noexcept;

  // general API

  template <typename T = string> const std::pair<string, T> key_val(txt_opt opt) const {
    const auto &[key, val] = lookup_map.at(opt);

    if constexpr (std::is_same_v<T, string>) {
      return std::make_pair(key, val);
    } else if constexpr (std::is_integral_v<std::remove_cvref_t<T>>) {
      T num{0};
      auto result{std::from_chars(val.data(), val.data() + val.size(), num)};

      if (result.ec == std::errc()) {
        return std::make_pair(key, num);
      } else {
        throw(std::runtime_error("not an integral type"));
      }
    } else {
      static_assert(always_false_v<T>, "unhandled type");
    }
  }

  // lookup up a key / value pair of a txt_opt
  const auto &lookup(const txt_opt opt) const noexcept { return lookup_map.at(opt); }

  const string make_txt_string(const txt_type type,
                               string_view sep = string_view()) const noexcept {
    const auto &service_def = service_map.at(type);

    return make_txt_string(service_def.order, sep);
  }

  // create the txt string key/val pair string from using the sequence of txt_opts
  const string make_txt_string(const txt_opt_seq_t &order,
                               string_view sep = string_view()) const noexcept {
    std::vector<string> parts; // contains strings of "key=val"

    for (const auto &opt : order) {
      const auto &entry = lookup_map.at(opt);
      parts.emplace_back(fmt::format("{}={}", entry.first, entry.second));
    }

    string txt_val_string;
    auto w = std::back_inserter(txt_val_string);

    for (const auto &p : parts) {
      if (!txt_val_string.empty() && !sep.empty()) { // add the separator, if needed
        fmt::format_to(w, "{}", sep);
      } else {
        fmt::format_to(w, "{}", p); // add the key/val
      }
    }

    return txt_val_string;
  }

  std::vector<string> make_txt_entries(const txt_type type) const noexcept {
    std::vector<string> entries;

    const auto &service_def = service_map.at(type);

    for (const auto &opt : service_def.order) {
      const auto &entry = lookup_map.at(opt);
      entries.emplace_back(fmt::format("{}={}", entry.first, entry.second));
    }

    return entries;
  }

  std::vector<txt_kv_t> key_val_for_type(const txt_type type) const noexcept {
    std::vector<txt_kv_t> entries;

    const auto &service_def = service_map.at(type);

    for (const auto &opt : service_def.order) {
      const auto &entry = lookup_map.at(opt);
      entries.emplace_back(entry);
    }

    return entries;
  }

  txt_kv_t name_and_reg(txt_type type) const noexcept {
    const auto opt = (type == txt_type::AirPlayTCP) //
                         ? txt_opt::AirPlayRegNameType
                         : txt_opt::RaopRegNameType;

    return key_val(opt);
  }

  // set the status flag to indicate if the receiver is active or inactive
  void receiver_active(bool active = true) noexcept {

    if (active == true) {
      _status_flags.rendering();
    } else {
      _status_flags.ready();
    }

    update_system_flags(); // reflect the status flags changes
  }

  template <typename T> void update_key_val(txt_opt opt, T new_val) noexcept {
    using U = std::remove_cvref_t<T>;

    auto &[key, val] = lookup_map[opt];

    if constexpr (std::is_same_v<U, string>) {
      val = new_val;
    } else if constexpr (std::is_integral_v<U>) {
      val = fmt::format("{}", new_val);
    } else if constexpr (std::is_same_v<T, uint8v>) {
      val = fmt::format("{:02x}", fmt::join(new_val, ""));
    } else {
      val = string(new_val);
    }
  }

private:
  void update_system_flags() noexcept {

    constexpr auto opts = std::array{txt_opt::apSystemFlags, //
                                     txt_opt::apStatusFlags, //
                                     txt_opt::mdSystemFlags};

    for (auto opt : opts) {
      const auto str = fmt::format("{:#x}", _status_flags.val());

      update_key_val(opt, str);
    }
  }

private:
  StatusFlags _status_flags; // see status_flags.hpp

  static lookup_map_t lookup_map;
  static service_txt_map service_map;

public:
  static constexpr csv module_id{"mdns::SERVICE"};
};
} // namespace pierre