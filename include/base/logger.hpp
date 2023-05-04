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

#include "base/asio.hpp"
#include "base/conf/token.hpp"
#include "base/elapsed.hpp"
#include "base/pet_types.hpp"
#include "base/types.hpp"

#include <algorithm>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/os.h>
#include <memory>

namespace pierre {

class Logger;

extern std::unique_ptr<Logger> _logger;

class Logger {

public:
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;

public:
  Logger(asio::io_context &app_io_ctx) noexcept;
  Logger(const Logger &) = delete;
  Logger(Logger &) = delete;
  Logger(Logger &&) = delete;

  ~Logger() noexcept;

  static Logger *create(asio::io_context &app_io_ctx) noexcept {
    _logger = std::make_unique<Logger>(app_io_ctx);

    return _logger.get();
  }

  template <typename M, typename C, typename S, typename... Args>
  void info(const M &mod_id, const C &cat, const S &format, Args &&...args) {

    if (should_log(mod_id, cat)) {
      const auto runtime{std::chrono::duration_cast<millis_fp>(e.as<Nanos>())};

      const auto prefix = fmt::format(prefix_format,      //
                                      runtime,            // millis since app start
                                      width_ts,           // width of timestamp field
                                      width_ts_precision, // runtime + width and precision
                                      mod_id, width_mod,  // module_id + width
                                      cat, width_cat);    // category + width)

      auto msg = fmt::vformat(format, fmt::make_format_args(args...));

      if (msg.back() != '\n') msg.append("\n");

      if (async_active && !app_io_ctx.stopped()) {
        asio::post(app_io_ctx, [this, prefix = std::move(prefix), msg = std::move(msg)]() {
          out.print("{} {}", prefix, msg);
          out.flush();
        });

      } else {
        out.print("{} {}", prefix, msg);
        out.flush();
      }
    }
  }

  bool should_log(csv mod, csv cat) noexcept {

    if (tokc->changed()) {
      asio::post(app_io_ctx, [this]() {
        tokc->latest();

        info(module_id, "conf"sv, "accepted {}", *tokc);
      });
    }

    if ((cat == csv{"info"}) || !tokc->is_table() || tokc->empty()) return true;

    // order of precedence:
    //  1. looger.<cat>       == boolean
    //  2. logger.<mod>       == boolean
    //  3. logger.<mod>.<cat> == boolean
    std::array paths{toml::path(cat), toml::path(mod), toml::path(mod).append(cat)};

    return std::all_of(paths.begin(), paths.end(), [t = tokc->table()](const auto &p) {
      const auto node = t->at_path(p);

      return node.is_boolean() ? node.value_or(true) : true;
    });
  }

  static void shutdown() noexcept { _logger.reset(); }
  static void synchronous() noexcept { _logger->async_active = false; }

private:
  // order dependent
  conf::token *tokc;
  asio::io_context &app_io_ctx;
  fmt::ostream out;

  // order independent
  bool async_active{false};
  static Elapsed e;

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

#define INFO(__cat, format, ...)                                                                   \
  pierre::_logger->info(module_id, __cat, FMT_STRING(format), ##__VA_ARGS__);

#define INFO_AUTO_CAT(cat)                                                                         \
  static constexpr std::string_view fn_id { cat }

#define INFO_AUTO(format, ...)                                                                     \
  pierre::_logger->info(module_id, fn_id, FMT_STRING(format), ##__VA_ARGS__);

#define INFO_INIT(format, ...)                                                                     \
  pierre::_logger->info(module_id, "init"sv, FMT_STRING(format), ##__VA_ARGS__);

#define INFO_MODULE_ID(mid)                                                                        \
  static constexpr std::string_view module_id { mid }

inline constexpr bool SHOULD_LOG(auto mid, auto cat) noexcept {
  return _logger ? _logger->should_log(mid, cat) : true;
}

} // namespace pierre
