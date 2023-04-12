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

#include "base/pet_types.hpp"
#include "base/types.hpp"

#include <any>
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system.hpp>
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

namespace asio = boost::asio;

class Logger;

namespace shared {
extern std::unique_ptr<Logger> logger;
}

class Logger {
private:
  using strand_ioc = boost::asio::strand<boost::asio::io_context::executor_type>;

public:
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;

public:
  Logger() noexcept;
  Logger(const Logger &) = delete;
  Logger(Logger &) = delete;
  Logger(Logger &&) = delete;

  ~Logger() noexcept;

  static void async(asio::io_context &io_ctx) noexcept;

  template <typename M, typename C, typename S, typename... Args>
  void info(const M &mod_id, const C &cat, const S &format, Args &&...args) {

    if (should_log(mod_id, cat)) {

      const auto prefix = fmt::format(prefix_format,      //
                                      runtime(),          // millis since app start
                                      width_ts,           // width of timestamp field
                                      width_ts_precision, // runtime + width and precision
                                      mod_id, width_mod,  // module_id + width
                                      cat, width_cat);    // category + width)

      const auto msg = fmt::vformat(format, fmt::make_format_args(args...));

      if (local_strand.has_value() && !local_strand->get_inner_executor().context().stopped()) {
        asio::post(*local_strand, [this, prefix = std::move(prefix), msg = std::move(msg)]() {
          print(std::move(prefix), std::move(msg));
        });

      } else {
        print(std::move(prefix), std::move(msg));
      }
    }
  }

  millis_fp runtime() noexcept;

  static bool should_log(csv mod, csv cat) noexcept; // see .cpp

private:
  void print(const string prefix, const string msg) noexcept;

private:
  // order independent
  bool shutting_down{false};
  std::optional<strand_ioc> local_strand;
  std::optional<fmt::ostream> out;
  std::any cfg_table;

public:
  // order independent
  static constexpr csv SPACE{" "};
  static constexpr fmt::string_view prefix_format{"{:>{}.{}} {:<{}} {:<{}}"};
  static constexpr int width_cat{15};
  static constexpr int width_mod{18};
  static constexpr int width_ts_precision{1};
  static constexpr int width_ts{13};

public:
  static constexpr csv module_id{"logger"};
};

#define INFO(mod_id, cat, format, ...)                                                             \
  pierre::shared::logger->info(mod_id, cat, FMT_STRING(format), ##__VA_ARGS__)

#define INFO_AUTO(format, ...)                                                                     \
  pierre::shared::logger->info(module_id, fn_id, FMT_STRING(format), ##__VA_ARGS__)

#define INFO_INIT(format, ...)                                                                     \
  pierre::shared::logger->info(module_id, csv{"init"}, FMT_STRING(format), ##__VA_ARGS__)

} // namespace pierre
