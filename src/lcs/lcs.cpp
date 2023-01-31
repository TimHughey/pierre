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

#include "lcs/lcs.hpp"
#include "lcs/args.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"

#include <algorithm>
#include <boost/program_options.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <latch>
#include <memory>

namespace pierre {

void LoggerConfigStats::init(io_context &io_ctx, CliArgs cli_args) noexcept {

   // if we've made it here then Config is parsed and ready; start our thread
  // auto latch = std::make_shared<std::latch>(1);

  // threads.emplace_back([this, latch]() mutable {
  //   name_thread("lcs");

  //   Logger::init(io_ctx);
  //   Stats::init(io_ctx);

  //   latch->arrive_and_wait();
  //   latch.reset();

  //   io_ctx.run();
  // });

  // latch->wait();

  // INFO(module_id, "info", "{}\n", Config::ptr()->banner_msg);
}

void LoggerConfigStats::shutdown() noexcept {
  // static constexpr csv fn_id{"shutdown"};

  // INFO(module_id, fn_id, "shutdown requested\n");

  Config::shutdown();
  Logger::shutdown();

  // guard.reset();
  // io_ctx.stop();

  // std::erase_if(threads, [](auto &t) {
  //   INFO(module_id, fn_id, "joining thread={}\n", t.get_id());

  //   t.join();
  //   return true;
  // });

  // INFO(module_id, fn_id, "shutdown complete, threads={}\n", std::ssize(threads));
}

} // namespace pierre