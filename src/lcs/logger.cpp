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
#include "lcs/config/watch.hpp"

#include <algorithm>
#include <boost/asio/append.hpp>
#include <boost/asio/post.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

namespace pierre {

namespace asio = boost::asio;

namespace shared {
std::unique_ptr<Logger> logger;
}

Elapsed elapsed_runtime;

namespace fs = std::filesystem;

Logger::Logger() noexcept : ctoken(module_id) {

  const fs::path path{Config::daemon() ? "/var/log/pierre/pierre.log" : "/dev/stdout"};
  const auto flags = fmt::file::WRONLY | fmt::file::APPEND | fmt::file::CREATE;
  out.emplace(fmt::output_file(path.c_str(), flags));

  const auto now = std::chrono::system_clock::now();
  out->print("\n{:%FT%H:%M:%S} START\n", now);

  shared::config->copy(module_id, ctoken.table);
}

Logger::~Logger() noexcept {
  static constexpr csv fn_id{"shutdown"};

  INFO_AUTO("shutdown requested\n");
}

void Logger::async(asio::io_context &io_ctx) noexcept { // static
  static constexpr csv fn_id{"info"};

  auto self = shared::logger.get();

  // create our local strand to serialized logging
  self->local_strand.emplace(asio::make_strand(io_ctx));

  // handler to process config changes
  auto handler = [self](std::any &&next_table) {
    auto &strand = self->local_strand.value();

    // for clarity build the post action
    auto action = asio::append([self](std::any t) { self->ctoken.table = std::move(t); },
                               std::move(next_table));

    asio::post(strand, std::move(action));
  };

  config_watch().register_token(self->ctoken, std::move(handler));

  // add a thread to the injected io_ctx to support our strand
  std::jthread([io_ctx = std::ref(io_ctx)]() { io_ctx.get().run(); }).detach();
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
  static constexpr csv fn_id{"should_log"};

  if (cat == csv{"info"}) return true;

  const auto &t = shared::logger->ctoken.get<toml::table>();

  // order of precedence:
  //  1. looger.<cat>       == boolean
  //  2. logger.<mod>       == boolean
  //  3. logger.<mod>.<cat> == boolean
  std::array paths{toml::path(cat), toml::path(mod), toml::path(mod).append(cat)};

  return std::all_of(paths.begin(), paths.end(), [&t = t](const auto &p) {
    const auto node = t.at_path(p);

    if (node.is_boolean()) {
      return node.value_or(true);
    } else {
      return true;
    }
  });
}

} // namespace pierre
