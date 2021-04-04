/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#ifndef _pierre_cli_subcmd_hpp
#define _pierre_cli_subcmd_hpp

#include <memory>
#include <string>

#include "core/state.hpp"

namespace pierre {
namespace cli {

class SubCmd {
public:
  using string = std::string;
  enum { finished = -255 };

public:
  SubCmd() = default;
  SubCmd(const SubCmd &) = delete;
  SubCmd &operator=(const SubCmd &) = delete;

  virtual int handleCmd(const string &args) = 0;
  virtual const string &name() const = 0;
};

typedef std::unique_ptr<SubCmd> upSubCmd;
} // namespace cli
} // namespace pierre

#endif
