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

#pragma once

#include "packet/aplist.hpp"
#include "reply/reply.hpp"

namespace pierre {
namespace airplay {
namespace reply {

class Teardown : public Reply {
public:
  Teardown() : rdict(packet::Aplist::DEFER_DICT) {}

  bool populate() override;

private:
  void phase1();
  void phase2();

private:
  packet::Aplist rdict;
};

} // namespace reply
} // namespace airplay
} // namespace pierre