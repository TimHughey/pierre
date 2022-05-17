/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "common/typedefs.hpp"
#include "core/typedefs.hpp"
#include "packet/aplist.hpp"
#include "reply/dict_keys.hpp"
#include "reply/reply.hpp"

#include <unordered_map>
#include <vector>

namespace pierre {
namespace airplay {
namespace reply {

namespace { // anoanymous namespace limits scope to this file
using Aplist = packet::Aplist;
}

class Setup : public Reply {
public:
  Setup() : rdict(Aplist::DEFER_DICT), reply_dict() {}

  bool populate() override;

private:
  bool handleNoStreams();
  bool handleStreams();

private:
  Aplist rdict;      // the request dict
  Aplist reply_dict; // entire reply dict

  StreamData stream_data;
};

} // namespace reply
} // namespace airplay
} // namespace pierre