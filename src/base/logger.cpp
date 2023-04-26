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

#include "logger.hpp"
#include "base/conf/fixed.hpp"
#include "elapsed.hpp"

namespace pierre {
std::unique_ptr<Logger> _logger;

namespace asio = boost::asio;

Elapsed Logger::e;

static const auto out_path() noexcept {
  return string(conf::fixed::daemon() ? conf::fixed::log_file() : "/dev/stdout");
}

namespace fs = std::filesystem;
static constexpr auto flags{fmt::file::WRONLY | fmt::file::APPEND | fmt::file::CREATE};

Logger::Logger(asio::io_context &app_io_ctx) noexcept
    : conf::token(module_id), app_io_ctx(app_io_ctx), //
      out(fmt::output_file(out_path(), flags))        //
{
  const auto now = std::chrono::system_clock::now();
  out.print("\n{:%FT%H:%M:%S} START\n", now);

  asio::post(app_io_ctx, [this]() { async_active = true; });
}

Logger::~Logger() noexcept {
  async_active = false;

  const auto now = std::chrono::system_clock::now();
  out.print("\n{:%FT%H:%M:%S} STOP\n", now);
  out.close();
}

} // namespace pierre
