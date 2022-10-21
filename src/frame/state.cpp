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

#include "state.hpp"

#include <map>

namespace pierre {
namespace frame {

csv state::inspect() const noexcept {
  static std::map<state_now_t, ccs> //
      val_to_txt_map = {{DECIPHERED, "deciphered"},
                        {DECIPHER_FAILURE, "decipher falure"},
                        {DECODED, "decoded"},
                        {DECODE_FAILURE, "decode failure"},
                        {DSP_IN_PROGRESS, "dsp in progress"},
                        {DSP_COMPLETE, "dsp complete"},
                        {EMPTY, "empty"},
                        {ERROR, "error"},
                        {FLUSHED, "flushed"},
                        {FUTURE, "future"},
                        {HEADER_PARSED, "header parsed"},
                        {INVALID, "invalid"},
                        {NO_SHARED_KEY, "no shared key"},
                        {NONE, "none"},
                        {OUTDATED, "outdated"},
                        {PARSE_FAILURE, "parse failure"},
                        {READY, "ready"},
                        {RENDERED, "rendered"}};

  return csv(val_to_txt_map[_val]);
}

} // namespace frame
} // namespace pierre
