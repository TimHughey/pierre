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

#pragma once

#include "base/conf/token.hpp"
#include "base/conf/toml.hpp"
#include "base/dura_t.hpp"
#include "base/types.hpp"
#include "base/uuid.hpp"

#include <array>
#include <concepts>
#include <fmt/format.h>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>

namespace pierre {
namespace conf {

class watch;

/// @brief Provides access to configuration info using
///        the specified module id as the root.
///
///        conf::tokens are generally not used standalone.
///        Rather, they are member variables within an
///        object that requires access to the configuration
///        file.
///
///        The configuration data provided is current as of
///        the time of construction.
///
///        In other words, if the configuration has changed
///        between startup and construction the most recent
///        version is captured in the newly create token.
///
///        See conf::watch for objects that require
///        notification of configuration file changes
///
///        conf::token can supply tokens that provided
///        notifications via conf::token::acquire_watch_token
///
///        conf::token acquired via conf::token::acquire_watch_token
///        must be a pointer member variable and the user must
///        call release in the object's destructor to prevent
///        memory leaks.

class token {
  friend struct fmt::formatter<token>;
  friend class watch;

public:
  /// @brief Create a default token (does not point to a configuration)
  token() = default;

  /// @brief Create config token with populated subtable that is not registered
  ///        for change notifications
  /// @param mid module_id (aka root)
  token(csv mid) noexcept;

  /// @brief Create config token with populated subtable that is registered
  ///        for change notifications
  /// @param mid  module id (aka root)
  /// @param watcher pointer to watcher
  token(csv mid, watch *watcher) noexcept;

  ~token() noexcept;

  /// @brief Move constructor
  /// @param other objec to move from
  token(token &&other) = default;

  /// @brief Move assignment operator
  /// @param lhs object to move assign
  /// @return reference to moved object
  token &operator=(token &&) = default;

public:
  /// @brief Acquire a watch token extension that provides
  ///        notifications of changes to the on-disk
  ///        configuration file.
  /// @param mid module id (used as root of configuration)
  /// @return conf::token with embedded watcher object
  static token *acquire_watch_token(csv mid) noexcept;

  /// @brief Has the configuration maintained by the token changed?
  /// @return boolean
  bool changed() noexcept { return std::exchange(has_changed, false); }

  /// @brief Is the configuration provided by this token empty?
  /// @return boolean
  bool empty() const noexcept { return ttable.empty(); }

  /// @brief Populates this token with the latest configuration
  ///        when a watcher is embedded.
  ///
  ///        NOTE: may block if watcher is busy reading the
  ///        changed on-disk configuration file
  /// @return boolean indicating if the stored configuration
  ///         is valid
  bool latest() noexcept;

  /// @brief Initiate watching for tokens with an emedded watcher
  ///        otherwise a no-op
  ///
  ///        NOTE: this method must be called once for tokens with
  ///        an embedded watcher
  void initiate_watch() noexcept;

  /// @brief Confirm the configuration availabe via the token is
  ///        a table
  /// @return boolean
  bool is_table() const noexcept { return ttable[root].is_table(); }

  /// @brief Retrieve parser error messages or empty string if there
  ///        are none
  ///
  ///        This method is provided primary for use during startup
  ///        and before Logger has been started
  /// @param id message id
  /// @return const string
  const string &msg(ParseMsg id) const noexcept { return msgs[id]; }

  /// @brief Retrieve configuration value located at path
  /// @tparam ReturnType Desired time of the returned value
  /// @param p Path to value, excluding root (a.k.a. module_id)
  /// @param def_val Default value if no value found at specified path
  ///                Default value is converted to ReturnType
  /// @return value of type ReturnType at specified path or provided default value
  template <typename ReturnType> inline ReturnType val(auto &&p, const auto &&def_val) noexcept {
    const auto path = root + p;

    if constexpr (IsDuration<ReturnType>) {
      return ReturnType(ttable[path].value_or(def_val));
    } else if constexpr (std::same_as<ReturnType, decltype(def_val)>) {
      return ttable[path].value_or(std::forward<decltype(def_val)>(def_val));
    } else if constexpr (std::constructible_from<ReturnType, decltype(def_val)>) {
      return ttable[path].value_or(ReturnType{def_val});
    } else {
      return ttable[path].value_or(std::forward<ReturnType>(def_val));
    }
  }

  /// @brief Helper function to determine if parsing was successful
  /// @return boolean
  bool parse_ok() const noexcept { return msgs[Parser].empty(); }

  /// @brief Release an acquired watch token
  void release() const noexcept;

  /// @brief Direct access to configuration table managed by token
  ///        Use with caution for access to configuration not handled
  ///        by member functions (e.g. array)
  /// @return
  toml::table *table() noexcept { return ttable[root].as<toml::table>(); }

  /// @brief Retrieve a "timeout" value from the config specified as:
  ///        silence = { timeout = {mins = 5, secs = 30, millis = 100 } }
  /// @param p path to the config value
  /// @param def_val default duration
  /// @return std::chrono::milliseconds
  Millis timeout_val(toml::path &&p, const auto &&def_val) noexcept {
    const auto path = root + p + "timeout"_tpath;

    Millis sum_ms{0};

    if (ttable[path].is_table()) {
      auto timeout_table = ttable[path].ref<toml::table>();

      timeout_table.for_each([&sum_ms](const toml::key &key, const toml::value<int64_t> &val) {
        const int64_t v = val.get();

        if ((key == "minutes"sv) || (key == "min"sv)) {
          sum_ms += Minutes{v};
        } else if ((key == "seconds"sv) || (key == "secs"sv)) {
          sum_ms += Seconds{v};
        } else if ((key == "millis"sv) || (key == "ms"sv)) {
          sum_ms += Millis{v};
        }
      });
    } else {
      sum_ms = std::chrono::duration_cast<Millis>(def_val);
    }

    return sum_ms;
  }

protected:
  /// @brief Helper method for populating parser related messages (e.g. errors)
  /// @param msg_id The message id to populate
  /// @param m string (copied)
  void add_msg(ParseMsg msg_id, const string m) noexcept { msgs[msg_id] = m; }
  bool changed(bool b) noexcept { return std::exchange(has_changed, b); }

protected:
  UUID uuid;
  toml::path root;
  toml::table ttable;
  parse_msgs_t msgs;

  bool has_changed{false};
  watch *watcher{nullptr};

public:
  static constexpr csv module_id{"conf.token"};
};

} // namespace conf
} // namespace pierre

template <> struct fmt::formatter<pierre::conf::token> {

  // Presentation format: 'f' - full, 's' - short (default)
  char presentation = 0x00;

  // Parses format specifications of the form ['f' | 's'].
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    // [ctx.begin(), ctx.end()) is a character range that contains a part of
    // the format string starting from the format specifications to be parsed,
    // e.g. in
    //
    //   fmt::format("{:f} - point of interest", point{1, 2});
    //
    // the range will contain "f} - point of interest". The formatter should
    // parse specifiers until '}' or the end of the range. In this example
    // the formatter should parse the 'f' specifier and return an iterator
    // pointing to '}'.

    // Please also note that this character range may be empty, in case of
    // the "{}" format string, so therefore you should check ctx.begin()
    // for equality with ctx.end().

    // Parse the presentation format and store it in the formatter:

    auto it = ctx.begin(), end = ctx.end();

    if (it != end) {

      if (*it == '}') presentation = 's';

      if ((*it == 'f') || (*it == 's')) presentation = *it++;
    }

    if (!presentation && (it != end) && (*it != '}')) throw format_error("invalid format");

    return it;
  }

  // parse is inherited from formatter<string>.
  template <typename FormatContext>
  auto format(const pierre::conf::token &tok, FormatContext &ctx) const {

    std::string msg;
    auto w = std::back_inserter(msg);

    fmt::format_to(w, "root={} uuid={:s}", tok.root.str(), tok.uuid);

    if (tok.is_table()) {
      const auto &ttable = tok.ttable[tok.root].ref<toml::table>();
      fmt::format_to(w, " size={}", ttable.size());
    } else {
      fmt::format_to(w, " **ROOT NOT FOUND**");
    }

    // return presentation == 'f'
    //           ? fmt::format_to(ctx.out(), "({:.1f}, {:.1f})", p.x, p.y)
    //           : fmt::format_to(ctx.out(), "({:.1e}, {:.1e})", p.x, p.y);

    return fmt::format_to(ctx.out(), "{}", msg);
  }
};