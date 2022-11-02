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
#include "config/config.hpp"
#include "base/elapsed.hpp"
#include "base/host.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"
#include "config/args.hpp"
#include "version.h"

#include <filesystem>
#include <latch>
#include <memory>

namespace pierre {

namespace {
namespace fs = std::filesystem;
}

// static data outside of class
static io_context io_ctx;
static std::shared_ptr<work_guard> guard;

// class static member data
toml::table Config::_table;
bool Config::initialized{false};
Threads Config::threads;
stop_tokens Config::tokens;

// initialize static data
void Config::init_self(int argc, char **argv) noexcept {
  // parse args
  Args args;
  const auto am = args.parse(argc, argv);

  if (am.ok()) {
    fs::path full_path{"/home/thughey/.pierre"};

    full_path /= am.cfg_file;

    try {
      _table = toml::parse_file(full_path.c_str());

      table_at("cli")->emplace<string>("app_name"sv, basename(argv[0]));
      table_at("cli")->emplace("cfg_file"sv, am.cfg_file);

      table_at("base")->emplace<string>("build_vsn"sv, GIT_REVISION);
      table_at("base")->emplace<string>("build_time"sv, BUILD_TIMESTAMP);

      initialized = true;

    } catch (const toml::parse_error &err) {
      INFO(module_id, "ERROR", "parse failed file={} reason={}\n", full_path.c_str(),
           err.description());
    }

    if (initialized) {
      guard = std::make_shared<work_guard>(io_ctx.get_executor());

      std::latch latch(CONFIG_THREADS);

      // note: work guard created by constructor p
      for (auto n = 0; n < CONFIG_THREADS; n++) { // main thread is 0s
        threads.emplace_back([=, &latch](std::stop_token token) mutable {
          tokens.add(std::move(token));
          name_thread(TASK_NAME, n);
          latch.arrive_and_wait();
          io_ctx.run();
        });
      }

      // caller thread waits until all tasks are started
      latch.wait();
      INFO(module_id, "INIT", "sizeof={} threads={}\n", //
           sizeof(Config), threads.size());
    }
  }
}

const string Config::receiver() const noexcept {
  static constexpr csv fallback{"%h"};
  auto val = _table.at_path("mdns.receiver").value_or(fallback);

  return (val == fallback ? string(Host().hostname()) : string(val));
}

toml::table *Config::table_at(csv path) { return _table.at_path(path).as_table(); }

} // namespace pierre