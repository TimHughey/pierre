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

#include "reply/flush.hpp"
#include "aplist/aplist.hpp"
#include "frame/flush_info.hpp"
#include "frame/racked.hpp"
#include "reply/dict_keys.hpp"

namespace pierre {
namespace airplay {
namespace reply {

bool FlushBuffered::populate() {
  rdict = plist();

  if (false) { // debug
    rHeaders().dump();
    rdict.dump("FlushBuffered::populate");
  }

  // from_seq and from_ts may not be present
  // until_seq and until_ts should always be present
  Racked::flush(                                   //
      FlushInfo(rdict.uint({dk::FLUSH_FROM_SEQ}),  //
                rdict.uint({dk::FLUSH_FROM_TS}),   //
                rdict.uint({dk::FLUSH_UNTIL_SEQ}), //
                rdict.uint({dk::FLUSH_UNTIL_TS}))  //
  );

  responseCode(RespCode::OK);
  return true;
}

} // namespace reply
} // namespace airplay
} // namespace pierre
