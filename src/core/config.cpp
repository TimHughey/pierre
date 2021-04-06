/*
    Pierre - Custom Light Show for Wiss Landing
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

#define TOML_IMPLEMENTATION

#include "core/config.hpp"
#include "external/toml.hpp"

using namespace toml;
using namespace std;

namespace pierre {
namespace core {
Config::Config(path &file) : _file(file) { _tbl = parse_file(file.c_str()); }

toml::table *Config::dmx() { return _tbl["dmx"].as_table(); }

toml::table *Config::dsp(const string_view &subtable) {
  return _tbl["dsp"].as_table()->get(subtable)->as_table();
}

toml::table *Config::lightdesk() { return _tbl["lightdesk"].as_table(); }
toml::table *Config::pcm(const std::string_view &subtable) {
  return _tbl["pcm"].as_table()->get(subtable)->as_table();
}

} // namespace core
} // namespace pierre
