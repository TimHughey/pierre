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

#include "rtsp/reply/teardown.hpp"
#include "core/service.hpp"
#include "mdns/mdns.hpp"
#include "packet/headers.hpp"
#include "rtp/rtp.hpp"

namespace pierre {
namespace rtsp {

using namespace packet;

Teardown::Teardown(const Reply::Opts &opts) : Reply(opts), packet::Aplist(plist()) {
  // does the plist contain a steams array?
  has_streams = dictItemExists(STREAMS);

  // rHeaders().dump();
  // dictDump();
}

bool Teardown::populate() {
  // get Rtp instance, we'll use it a copy of times
  auto rtp = Rtp::instance();

  service()->deviceSupportsRelay(false);
  mdns()->update();
  responseCode(packet::RespCode::OK); // always OK

  if (has_streams == true) {
    // when the plist contains streams we only want Teardown phase 1

    auto teardown = rtp->teardown(Rtp::TeardownPhase::One);
    teardown.get();
  } else {
    // otherwise we want both phases

    auto teardown1 = rtp->teardown(Rtp::TeardownPhase::One);
    teardown1.get();

    auto teardown2 = rtp->teardown(Rtp::TeardownPhase::Two);
    teardown2.get();

    // include connection was closed
    headers.add(Headers::Type2::ContentSimple, Headers::Val2::ConnectionClosed);
  }

  return true;
}
} // namespace rtsp
} // namespace pierre