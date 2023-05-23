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

const state &state::record_state() const noexcept {

  Stats::write<state>(pierre::stats::FRAME, 1, tag());

  return *this;
}

std::map<state_now_t, string> state::vt_map{
    // note: comment for easy IDE sorting
    {CAN_RENDER, "can_render"},
    {DECIPHER_FAIL, "decipher_fail"},
    {DECIPHERED, "deciphered"},
    {DECODE_FAIL, "decode_fail"},
    {DSP, "dsp"},
    {NO_AUDIO, "no_audio"},
    {ERROR, "error"},
    {FLUSHED, "flushed"},
    {FUTURE, "future"},
    {HEADER_PARSED, "header_parsed"},
    {INVALID, "invalid"},
    {MOVED, "moved"},
    {NO_CLK_ANC, "no_clk_anc"},
    {NO_SHARED_KEY, "no_shr_key"},
    {NONE, "none"},
    {OUTDATED, "outdated"},
    {PARSE_FAIL, "parse_fail"},
    {READY, "ready"},
    {RENDERED, "rendered"},
    {SILENCE, "silence"},
    // note: extra comma above for easy IDE sorting
};

} // namespace frame
} // namespace pierre
