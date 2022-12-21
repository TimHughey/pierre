//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/types.hpp"
#include "base/uint8v.hpp"

#include "fmt/format.h"
#include <charconv>
#include <exception>
#include <map>
#include <set>
#include <type_traits>

namespace pierre {

struct hdr_type {
  static const string AppleHKP;
  static const string AppleProtocolVersion;
  static const string ContentLength;
  static const string ContentSimple;
  static const string ContentType;
  static const string CSeq;
  static const string DacpActiveRemote;
  static const string DacpID;
  static const string Public;
  static const string RtpInfo;
  static const string Server;
  static const string UserAgent;
  static const string XAppleAbsoulteTime;
  static const string XAppleClientName;
  static const string XAppleET;
  static const string XAppleHKP;
  static const string XApplePD;
  static const string XAppleProtocolVersion;
};

struct hdr_val {
  static const string AirPierre;
  static const string AppleBinPlist;
  static const string ConnectionClosed;
  static const string ImagePng;
  static const string OctetStream;
  static const string TextParameters;
};

class Headers {

public:
  Headers() = default;
  Headers(const Headers &h) = default;
  Headers(Headers &&h) = default;

  Headers &operator=(const Headers &h) = default;
  Headers &operator=(Headers &&) = default;

  // early definitions for type deductions
public:
  /// @brief Add the specified header type and value
  /// @tparam T Integral type or string
  /// @param t Header Type (hdr_type)
  /// @param v Value
  /// @return auto
  template <typename T> auto add(const string &t, const T &v) noexcept {

    // converts integrals to strings or copies the string for emplacement
    auto to_string = [](const T &v) -> const string {
      if constexpr (std::is_integral_v<T>) {
        return fmt::format("{}", v);
      } else if constexpr (std::is_same_v<T, string>) {
        return string(v);
      } else {
        static_assert(always_false_v<T>, "unhandled type");
      }
    };

    if (known_types.contains(t)) {
      map.try_emplace(t, to_string(v));
    } else {
      unknown_headers.emplace(to_string(v));
    }
  }

  /// @brief Determine if a header type has a value
  /// @param t Header Type (hdr_type)
  /// @return boolean indicating header type has a value
  bool contains(const string &t) const noexcept { return map.contains(t); }

  /// @brief Format the known headers to the specified location
  /// @param where Any type accepted by fmt::format_to
  void format_to(auto &where) const noexcept {
    std::ranges::for_each(map, [&](const auto &entry) {
      fmt::format_to(where, "{}: {}\r\n", entry.first, entry.second);
    });
  }

  // get the value of a header type as a string (default) or an integral
  // may throw if header type does not exist in the map or the value can not be
  // converted to an integral value
  //
  // returns: a copy of the value of the header type

  /// @brief Get the value of a header
  /// @tparam T Type of desired return value
  /// @param t Header Type (hdr_type)
  /// @return Value of header
  /// @throws if header type does not exist or value can not be converted
  ///         to specified type
  template <typename T = string> const T val(const string &t) const {
    const auto &v = map.at(t);

    if constexpr (std::is_same_v<T, string>) {
      return v;
    } else if constexpr (std::is_integral_v<T>) {
      int64_t num{0};
      auto result{std::from_chars(v.data(), v.data() + v.size(), num)};

      if (result.ec == std::errc()) {
        return num;
      } else {
        throw(std::runtime_error("not an integral type"));
      }
    } else {
      static_assert(always_false_v<T>, "unhandled type");
    }
  }

  /// @brief Copy header value of specified header type into this object
  /// @param t Header Type (hdr_type)
  /// @param from Header object to copy value from
  void copy(const string &t, const Headers &from) noexcept { map.try_emplace(t, from.map.at(t)); }

  /// @brief Invoked one or more times to parse the packet headers
  /// @param packet Clear text packet to parse
  /// @param delims Delimiters of header section of packet.
  ///    Delimiter count decides if header can be parsed.
  ///    zero - immediately returns, not enough bytes to parse method;
  ///    one  - parses the method line
  ///    two  - parses the header block
  /// @return boolean indicating if parsing is complete
  bool parse(uint8v &packet, const uint8v::delims_t &delims) noexcept;

  // preamble info
  csv method() const noexcept { return csv{_method}; }
  csv path() const noexcept { return csv{_path}; }
  csv protocol() const noexcept { return csv{_protocol}; }

public:
  bool parse_ok{false};
  std::set<string> unknown_headers; // allow direct access to unknown headers

private:
  static std::set<string> known_types;
  std::map<string, string> map;

  string _method;
  string _path;
  string _protocol;

  static constexpr string_view EOL{"\r\n"};
  static constexpr string_view SEP{"\r\n\r\n"};

public:
  static constexpr csv module_id{"HEADERS"};
};

} // namespace pierre
