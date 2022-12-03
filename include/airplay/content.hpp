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

#include "base/types.hpp"
#include "base/uint8v.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pierre {

class Content : public uint8v {
public:
  Content() = default;

  bool operator==(const string &t) const noexcept { return t == type; }

  // misc debug
  void dump() const override;
  virtual csv moduleId() const override { return module_id; }

public:
  string type;

  static constexpr csv module_id{"CONTENT"};
};

} // namespace pierre