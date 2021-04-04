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

#ifndef _pierre_cli_subcmd_lightdesk_hpp
#define _pierre_cli_subcmd_lightdesk_hpp

#include <deque>
#include <string>

#include "cli/subcmds/subcmd.hpp"

namespace pierre {
namespace cli {

class LightDesk : public SubCmd {
public:
  using string = std::string;

public:
  LightDesk() = default;
  LightDesk(const LightDesk &) = delete;
  LightDesk &operator=(const LightDesk &) = delete;

  int handleCmd(const string &args) override;
  int handleMajorPeak();
  const string &name() const override {
    static const string x = "desk";
    return x;
  };

private:
  std::deque<string> tokens;
};

} // namespace cli
} // namespace pierre

#endif
