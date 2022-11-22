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

#include "base/io.hpp"
#include "base/types.hpp"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <stop_token>

namespace pierre {

class Logger;
namespace shared {
extern std::optional<Logger> logger;
}

#define INFO(mod_id, cat, format, ...)                                                             \
  pierre::shared::logger->info(mod_id, cat, FMT_STRING(format), ##__VA_ARGS__)

#define INFOX(mod_id, cat, format, ...)

#define INFO_FORMAT_CHUNK(data, size) pierre::shared::logger->format_chunk(data, size)
#define INFO_INDENT_CHUNK(chunk) pierre::shared::logger->indent_chunk(chunk)
#define INFO_TAB pierre::shared::logger->tab()
#define INFO_WITH_CHUNK(mod_id, cat, chunk, format, ...)                                           \
  pierre::shared::logger->info_with_chunk(mod_id, cat, chunk, FMT_STRING(format), ##__VA_ARGS__)

class Logger {
private:
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;

public:
  Logger() = default;

  const string format_chunk(ccs data, size_t bytes) const noexcept;

  void indent_chunk(csv chunk);

  template <typename M, typename C, typename S, typename... Args>
  void info(const M &mod_id, const C &cat, const S &format, Args &&...args) {

    if (!io_ctx.stopped()) {
      asio::post(io_ctx, [=, this,       //
                          e = runtime(), //
                          msg = fmt::vformat(
                              format, fmt::make_args_checked<Args...>(format, args...))]() mutable {
        // print the log message
        fmt::print(line_format,                     //
                   e, width_ts, width_ts_precision, // runtime + width and precision
                   mod_id, width_mod,               // module_id + width
                   cat, width_cat,                  // category + width
                   msg);

        if (stop_token.stop_requested()) guard.reset();
      });
    } else {
      fmt::print(line_format,                             //
                 runtime(), width_ts, width_ts_precision, //
                 mod_id, width_mod,                       //
                 cat, width_cat,
                 fmt::vformat(format, fmt::make_args_checked<Args...>(format, args...)));
    }
  }

  template <typename M, typename C, typename S, typename CK, typename... Args>
  void info_with_chunk(const M &mod_id, const C &cat, const CK &chunk, const S &format,
                       Args &&...args) {

    if (!io_ctx.stopped()) {
      asio::post(io_ctx, [=, this,       //
                          e = runtime(), //
                          chunk = string(std::data(chunk), std::size(chunk)),
                          msg = fmt::vformat(
                              format, fmt::make_args_checked<Args...>(format, args...))]() mutable {
        // print the initial log message with prefix
        fmt::print(line_format,                     //
                   e, width_ts, width_ts_precision, // runtime + width and precision
                   mod_id, width_mod,               // module_id + width
                   cat, width_cat,                  // category + width
                   msg);

        // print the chunk, indented
        print_chunk(chunk);

        if (stop_token.stop_requested()) guard.reset();
      });
    } else {
      fmt::print(line_format,                             //
                 runtime(), width_ts, width_ts_precision, //
                 mod_id, width_mod,                       //
                 cat, width_cat,
                 fmt::vformat(format, fmt::make_args_checked<Args...>(format, args...)));
    }
  }

  static void init();

  const string &tab() const noexcept { return indent; }

private:
  void init_self();
  void print_chunk(const string &chunk);
  static millis_fp runtime();

private:
  // order dependent
  io_context io_ctx;
  std::unique_ptr<work_guard> guard;
  Thread thread;

  // order independent
  std::stop_token stop_token;
  static constexpr fmt::string_view line_format{"{:>{}.{}} {:<{}} {:<{}} {}"};
  static constexpr csv SPACE{" "};
  static constexpr int width_ts{13};
  static constexpr int width_ts_precision{1};
  static constexpr int width_mod{18};
  static constexpr int width_cat{15};
  string indent;
};

} // namespace pierre
