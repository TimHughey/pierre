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

namespace pierre {
namespace core {

typedef std::chrono::steady_clock clock;
typedef std::chrono::steady_clock::time_point time_point;
typedef std::chrono::milliseconds milliseconds;
typedef std::error_code error_code;

class Config {
public:
  using path = std::filesystem::path;
  using string_view = std::string_view;

public:
  Config() = default;
  ~Config() = default;

  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

  const error_code &exists() const;
  const string_view fallback() const;

  auto operator[](const std::string_view &subtable);

  const error_code &parse(path &file, bool use_embedded = true);

  auto &table() { return _tbl; }

private:
  path _file;
  error_code _exists_rc;

  toml::table _tbl;
};

} // namespace core

using namespace core;
} // namespace pierre

#endif
