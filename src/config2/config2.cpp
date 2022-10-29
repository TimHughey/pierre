// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#define TOML_IMPLEMENTATION // this translation unit compiles actual library
#include "config2.hpp"
#include "base/elapsed.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"

#include <filesystem>

namespace pierre {

namespace {
namespace fs = std::filesystem;
}

// class static member data
toml::table C2onfig::_table;

C2onfig C2onfig::init(csv file) { // static
  fs::path full_path{"/home/thughey/.pierre"};

  full_path /= file;

  try {
    _table = toml::parse_file(full_path.c_str());

  } catch (const toml::parse_error &err) {
    INFO(module_id, "ERROR", "file={} parse failed={}\n", full_path.c_str(), err.description());
  }

  return C2onfig();
}

} // namespace pierre