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

#pragma once

#include "base/threads.hpp"
#include "base/types.hpp"
#include "io/io.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fmt/chrono.h>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/ostream.h>
#include <fmt/std.h>
#include <memory>
#include <optional>

namespace pierre {
class Logger;

namespace shared {
extern Logger logger;
}

class Logger {
private:
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;

public:
  Logger() noexcept;
  Logger(const Logger &) = delete;
  Logger(Logger &) = delete;
  Logger(Logger &&) = delete;

  template <typename M, typename C, typename S, typename... Args>
  void info(const M &mod_id, const C &cat, const S &format, Args &&...args) {

    if (should_log_info(mod_id, cat)) {

      const auto prefix = fmt::format(prefix_format,      //
                                      runtime(),          // millis since app start
                                      width_ts,           // width of timsstap field,
                                      width_ts_precision, // runtime + width and precision
                                      mod_id, width_mod,  // module_id + width
                                      cat, width_cat);    // category + width)

      const auto msg = fmt::vformat(format, fmt::make_format_args(args...));

      if (shutting_down.load() == false) {
        asio::post(io_ctx, [this, prefix = std::move(prefix), msg = std::move(msg)]() mutable {
          print(std::move(prefix), std::move(msg));
        });
      } else {
        print(std::move(prefix), std::move(msg));
      }
    }
  }

  millis_fp runtime() noexcept {
    return std::chrono::duration_cast<millis_fp>((Nanos)elapsed_runtime);
  }

  static bool should_log_info(csv mod, csv cat) noexcept; // see .cpp

  static void shutdown() noexcept { shared::logger.shutdown_impl(); }

  static void startup() noexcept { shared::logger.startup_impl(); }

private:
  void print(const string prefix, const string msg) noexcept;

  void shutdown_impl() noexcept;
  void startup_impl() noexcept;

private:
  // order dependent
  io_context io_ctx;
  work_guard guard;

  // order independent
  std::atomic_bool shutting_down{false};
  Threads threads;
  std::optional<fmt::ostream> out;

public:
  // order independent
  static constexpr csv SPACE{" "};
  static constexpr fmt::string_view prefix_format{"{:>{}.{}} {:<{}} {:<{}}"};
  static constexpr int width_cat{15};
  static constexpr int width_mod{18};
  static constexpr int width_ts_precision{1};
  static constexpr int width_ts{13};
  Elapsed elapsed_runtime;

public:
  static constexpr csv module_id{"logger"};
};

#define INFO(mod_id, cat, format, ...)                                                             \
  pierre::shared::logger.info(mod_id, cat, FMT_STRING(format), ##__VA_ARGS__)

#define INFOX(mod_id, cat, format, ...)

} // namespace pierre
