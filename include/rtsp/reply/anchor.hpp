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

#include <cstdint>

#include "rtsp/aplist.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {

class Anchor : public Reply, public Aplist {
public:
  Anchor(const Reply::Opts &opts);

  bool populate() override;

private:
  void calcRtpTime();

private:
  uint64_t _net_time_secs = 0;
  uint64_t _net_time_frac = 0;
  uint64_t _rtp_time = 0;
};

} // namespace rtsp
} // namespace pierre