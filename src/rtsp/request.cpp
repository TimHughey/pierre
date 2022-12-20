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

#include "rtsp/request.hpp"
#include "rtsp/aplist.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "headers.hpp"
#include "rtsp/ctx.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace pierre {
namespace rtsp {

void Request::save(const uint8v &wire) noexcept {

  if (Config().at("debug.rtsp.save"sv).value_or(false)) {
    const auto base = Config().at("debug.path"sv).value_or(string());
    const auto file = Config().at("debug.rtsp.file"sv).value_or(string());

    namespace fs = std::filesystem;
    fs::path path{fs::current_path()};
    path.append(base);

    try {
      fs::create_directories(path);

      fs::path full_path(path);
      full_path.append(file);

      std::ofstream out(full_path, std::ios::app);

      out.write(wire.raw<char>(), wire.size());

    } catch (const std::exception &e) {
      INFO(module_id, "RTSP_SAVE", "exception, {}\n", e.what());
    }
  }
}

} // namespace rtsp
} // namespace pierre
