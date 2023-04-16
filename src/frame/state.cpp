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
#include "base/stats.hpp"

namespace pierre {
namespace frame {

void state::record_state() const noexcept { Stats::write(pierre::stats::FRAME, 1, tag()); }

std::map<state_now_t, string> state::val_to_txt_map{
    // comment for easy IDE sorting
    {DECIPHER_FAILURE, "decipher_failure"},
    {DECIPHERED, "deciphered"},
    {DECODE_FAILURE, "decode_failure"},
    {DECODED, "decoded"},
    {DSP_COMPLETE, "dsp_complete"},
    {DSP_IN_PROGRESS, "dsp_in_progress"},
    {EMPTY, "empty"},
    {ERROR, "error"},
    {FLUSHED, "flushed"},
    {FUTURE, "future"},
    {HEADER_PARSED, "header_parsed"},
    {INVALID, "invalid"},
    {NO_SHARED_KEY, "no_shared_key"},
    {NONE, "none"},
    {OUTDATED, "outdated"},
    {PARSE_FAILURE, "parse_failure"},
    {READY, "ready"},
    {RENDERED, "rendered"},
    {SILENCE, "silence"}
    //
};

} // namespace frame
} // namespace pierre
