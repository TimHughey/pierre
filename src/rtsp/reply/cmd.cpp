
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

#include "rtsp/reply/cmd.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {
using namespace packet;

Command::Command(const Reply::Opts &opts) : Reply(opts), packet::Aplist(plist()) {
  debugFlag(false);

  // default to OK
  responseCode(packet::RespCode::OK);
}

bool Command::populate() {
  auto rc = true;

  // NOTE
  //
  // /command msgs come in two flavors:
  //  1. plist with specific keys

  rc &= dictEmpty(); // empty dictionary is OK

  rc &= (rc && checkUpdateSupportedCommands()); // check for dict entries

  return rc;
}

bool Command::checkUpdateSupportedCommands() {
  auto rc = true;

  rc &= dictCompareString("type", "updateMRSupportedCommands");

  if (rc) {
    // anytime params -> updateMRSupportedCommands is present
    // response code is bad request
    responseCode(RespCode::BadRequest);

    ArrayStrings array;

    rc &= dictGetStringArray("params", "mrSupportedCommandsFromSender", array);

    if (rc) {
      fmt::print("{} supported commands from sender", fnName());

      if (array.empty()) {
        fmt::print(" is empty\n");
      } else {
        fmt::print("\n");
        for (const auto &item : array) {
          fmt::print("\t\t{}\n", item);
        }
      }
    }
  }

  return rc;
}

} // namespace rtsp
} // namespace pierre