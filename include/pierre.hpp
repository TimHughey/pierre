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

#pragma once

#include "base/types.hpp"
#include "core/args.hpp"

#include <memory>
#include <string>
#include <tuple>

namespace pierre {

// forward decl for Pierre shared_ptr
class Pierre;
typedef std::shared_ptr<Pierre> shPierre;

class Pierre : public std::enable_shared_from_this<Pierre> {
private:
  struct Inject {
    csr app_name;
    const ArgsMap &args_map;
  };

public:
  ~Pierre() = default;

  static shPierre create(const Inject &di) {
    // Not using std::make_shared<Pierre>, constructor is private
    return shPierre(new Pierre(di));
  }

  static csv moduleID() { return module_id; }

  // main entry point
  void run();

private:
  Pierre(const Inject &di) : di(di) {}

private:
  Inject di;
  static constexpr csv module_id = "PIERRE";
};

} // namespace pierre
