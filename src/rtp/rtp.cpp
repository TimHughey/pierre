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

#include "rtp/rtp.hpp"
#include "rtp/anchor_info.hpp"
#include "rtp/buffered.hpp"
#include "rtp/control.hpp"
#include "rtp/event.hpp"

namespace pierre {
using namespace rtp;

Rtp::Rtp()
    : _event(rtp::Event::create()), _control(rtp::Control::create()),
      _buffered(rtp::Buffered::create()) {
  // more later
}

Rtp::~Rtp() {
  // more later
}

void Rtp::saveAnchorInfo(const rtp::AnchorData &data) {
  _anchor = data;
  _anchor.dump();
}

void Rtp::saveSessionInfo(csr shk, csr active_remote, csr dacp_id) {
  _session_key = shk;
  _active_remote = active_remote;
  _dacp_id = dacp_id;
}

} // namespace pierre