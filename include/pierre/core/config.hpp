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

#ifndef pierre_config_hpp
#define pierre_config_hpp

#include <atomic>
#include <chrono>
#include <cmath>
#include <ctgmath>
#include <filesystem>
#include <memory>
#include <string>

#define TOML_HEADER_ONLY 0
#include "external/toml.hpp"

using namespace std::string_view_literals;

namespace pierre {
namespace core {

typedef std::chrono::steady_clock clock;
typedef std::chrono::steady_clock::time_point time_point;
typedef std::chrono::milliseconds milliseconds;

class Config {
public:
  using path = std::filesystem::path;

public:
  Config(path &file);
  ~Config() = default;

  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

  toml::table *dmx();
  toml::table *dsp(const std::string_view &subtable);
  toml::table *lightdesk();
  toml::table *pcm(const std::string_view &subtable);

  toml::table &table() { return _tbl; }

private:
  path _file;

  toml::table _tbl;
};

} // namespace core

using namespace core;
} // namespace pierre

#endif
