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

#include "lcs/logger.hpp"
#include "base/elapsed.hpp"
#include "lcs/config.hpp"

#include <filesystem>
#include <iostream>

namespace pierre {

namespace shared {
std::unique_ptr<Logger> logger;
}

Elapsed elapsed_runtime;

namespace fs = std::filesystem;

Logger::Logger() noexcept : thread_pool(1) {

  const fs::path path{Config::daemon() ? "/var/log/pierre/pierre.log" : "/dev/stdout"};
  const auto flags = fmt::file::WRONLY | fmt::file::APPEND | fmt::file::CREATE;
  out.emplace(fmt::output_file(path.c_str(), flags));

  const auto now = std::chrono::system_clock::now();
  out->print("\n{:%FT%H:%M:%S} START\n", now);
}

Logger::~Logger() noexcept {
  static constexpr csv fn_id{"shutdown"};

  INFO(module_id, fn_id, "shutdown requested\n");

  thread_pool.stop();
  thread_pool.join();
}

void Logger::print(const string prefix, const string msg) noexcept {
  if (out.has_value()) {
    out->print("{} {}", prefix, msg);
    out->flush();
  } else {
    fmt::print(std::cout, "{} {}", prefix, msg);
  }
}

Logger::millis_fp Logger::runtime() noexcept { // static
  return std::chrono::duration_cast<millis_fp>((Nanos)elapsed_runtime);
}

bool Logger::should_log(csv mod, csv cat) noexcept { // static
  // in .cpp to avoid pulling config.hpp into Logger
  return cfg_logger(module_id, mod, cat);
}

} // namespace pierre
