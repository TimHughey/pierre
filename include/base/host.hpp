// Pierre - Custom audio processing for light shows at Wiss Landing
// Copyright (C) 2022  Tim Hughey
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "base/types.hpp"

#include <cstdint>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <vector>

namespace pierre {

using HwAddrBytes = std::array<uint8_t, 6>;
using Hostname = std::string;
using IpAddr = std::string;
using IpAddrs = std::vector<IpAddr>;
using PkBytes = std::array<uint8_t, 32>;

// Host Command Information
class Host {
public:
  Host() noexcept;

  // general API
  csv device_id() const noexcept { return csv{id.c_str()}; }
  csv hostname() const noexcept { return csv{name.c_str()}; }
  csv hw_address() const noexcept { return csv{hw_addr.c_str()}; }

  // IP address(es) of this host
  const HwAddrBytes &hw_address_bytes() const noexcept { return _hw_addr_bytes; }
  const IpAddrs &ip_addresses() const noexcept { return ip_addrs; }

  // without 0x prefix
  const string pk() const noexcept { return fmt::format("{:02x}", fmt::join(pk_bytes, "")); }
  ccs serial_num() const noexcept { return serial.c_str(); }

  // UUID for this host
  ccs uuid() const noexcept { return host_uuid.c_str(); }

private:
  void discover_ip_addrs();

public:
  // order dependent
  Hostname name;

  // order independent
  string id;
  string hw_addr;
  HwAddrBytes _hw_addr_bytes{0};
  IpAddrs ip_addrs;
  string serial;

  // static member data
  static string host_uuid;
  static PkBytes pk_bytes;

public:
  static constexpr csv module_id{"HOST"};
};

} // namespace pierre