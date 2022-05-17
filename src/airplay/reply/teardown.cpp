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

#include "reply/teardown.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "packet/headers.hpp"

namespace pierre {
namespace airplay {
namespace reply {

namespace header = pierre::packet::header;

bool Teardown::populate() {
  rdict = plist();

  headers.add(header::type::ContentSimple, header::val::ConnectionClosed);
  responseCode(packet::RespCode::OK); // always OK

  has_streams = rdict.exists(STREAMS);

  if (false) {
    constexpr auto f = FMT_STRING("{} {}\n");
    fmt::print(f, runTicks(), fnName());

    rdict.dump();
  }

  if (has_streams == true) {
    // when the plist contains streams we only want Teardown phase 1

    auto teardown = conn().teardown(TeardownPhase::One);
    teardown.get();
  } else {
    // otherwise we want both phases

    auto teardown1 = conn().teardown(TeardownPhase::One);
    teardown1.get();

    auto teardown2 = conn().teardown(TeardownPhase::Two);
    teardown2.get();

    Service::ptr()->deviceSupportsRelay(false);
    mDNS::ptr()->update();

    // include connection was closed
  }

  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre