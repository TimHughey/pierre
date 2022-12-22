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

class Logger : public std::enable_shared_from_this<Logger> {
private:
  using millis_fp = std::chrono::duration<double, std::chrono::milliseconds::period>;

private:
  Logger(io_context &io_ctx) noexcept               //
      : io_ctx(io_ctx),                             //
        local_strand(io_ctx),                       //
        indent(fmt::format("{:>{}} {:>{}} {:>{}} ", // pre-compute indent string
                           SPACE, width_ts, SPACE, width_mod, SPACE, width_cat)) {}

  auto ptr() noexcept { return shared_from_this(); }

public:
  const string format_chunk(ccs data, size_t bytes) const noexcept;

  template <typename M, typename C, typename S, typename... Args>
  void info(const M &mod_id, const C &cat, const S &format, Args &&...args) {

    if (!io_ctx.stopped()) {
      asio::post(
          local_strand,
          [=, s = ptr(), e = runtime(),
           msg = fmt::vformat(format, fmt::make_args_checked<Args...>(format, args...))]() mutable {
            // print the log message
            fmt::print(line_format,                     //
                       e, width_ts, width_ts_precision, // runtime + width and precision
                       mod_id, width_mod,               // module_id + width
                       cat, width_cat,                  // category + width
                       msg);
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
      asio::post(
          local_strand,
          [=, s = ptr(), e = runtime(), chunk = string(std::data(chunk), std::size(chunk)),
           msg = fmt::vformat(format, fmt::make_args_checked<Args...>(format, args...))]() mutable {
            // print the initial log message with prefix
            fmt::print(line_format,                     //
                       e, width_ts, width_ts_precision, // runtime + width and precision
                       mod_id, width_mod,               // module_id + width
                       cat, width_cat,                  // category + width
                       msg);

            // print the chunk, indented
            s->print_chunk(chunk);
          });
    } else {
      fmt::print(line_format,                             //
                 runtime(), width_ts, width_ts_precision, //
                 mod_id, width_mod,                       //
                 cat, width_cat,
                 fmt::vformat(format, fmt::make_args_checked<Args...>(format, args...)));
    }
  }

  /// @brief Initialize Logger subsystem
  /// @param io_ctx io_context for async logging when external cfg threads=0
  static void init(io_context &io_ctx) noexcept {
    self = std::shared_ptr<Logger>(new Logger(io_ctx));
  }

  const string &tab() const noexcept { return indent; }

  static void teardown() noexcept {
    auto s = self->ptr();                 // get a fresh shared_ptr to ourself
    auto &local_strand = s->local_strand; // get a reference to our strand, we'll move s

    self.reset(); // reset our static shared_ptr to ourself

    asio::post(local_strand, [s = std::move(s)]() {
      // keeps Logger in-scope until lambda completes
      // future implementation
    });
  }

private:
  void init_self();
  void print_chunk(const string &chunk);
  static millis_fp runtime();

private:
  // order dependent
  io_context &io_ctx;
  strand local_strand;
  const string indent;

  // order independent
  static constexpr csv SPACE{" "};
  static constexpr fmt::string_view line_format{"{:>{}.{}} {:<{}} {:<{}} {}"};
  static constexpr int width_cat{15};
  static constexpr int width_mod{18};
  static constexpr int width_ts_precision{1};
  static constexpr int width_ts{13};
  static Elapsed elapsed_runtime;

public:
  static std::shared_ptr<Logger> self; // must be public for macro
};

#define INFO(mod_id, cat, format, ...)                                                             \
  Logger::self->info(mod_id, cat, FMT_STRING(format), ##__VA_ARGS__)

#define INFOX(mod_id, cat, format, ...)

#define INFO_FORMAT_CHUNK(data, size) Logger::self->format_chunk(data, size)
#define INFO_INDENT_CHUNK(chunk) Logger::self->indent_chunk(chunk)
#define INFO_TAB Logger::self->tab()
#define INFO_WITH_CHUNK(mod_id, cat, chunk, format, ...)                                           \
  Logger::self->info_with_chunk(mod_id, cat, chunk, FMT_STRING(format), ##__VA_ARGS__)

} // namespace pierre
