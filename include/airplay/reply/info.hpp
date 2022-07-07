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

#include "base/aplist.hpp"
#include "reply/reply.hpp"

namespace pierre {
namespace airplay {
namespace reply {

enum InfoStage { StageUnknown, Stage1, Stage2 };

class Info : public Reply {
public:
  Info() : Reply("INFO"), rdict(Aplist::DEFER_DICT) {}

  bool populate() override;

private:
  bool stage1();
  bool stage2();

private:
  InfoStage _stage = StageUnknown;
  Aplist rdict;
};

} // namespace reply
} // namespace airplay
} // namespace pierre