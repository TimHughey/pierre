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
#include "base/dura.hpp"
#include "base/types.hpp"
#include "desk/msg/data.hpp"
#include "desk/units.hpp"
#include "frame/fdecls.hpp"
#include "fx/names.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system.hpp>
#include <initializer_list>
#include <memory>

namespace pierre {
namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using steady_timer = asio::steady_timer;
using strand_ioc = asio::strand<asio::io_context::executor_type>;

namespace desk {

class FX {

public:
  static auto constexpr NoRender{false};
  static auto constexpr Render{true};

public:
  /// @brief Construct the base FX via the subclassed type
  FX(auto &executor, const string name, const string next_fx = fx::NONE,
     bool should_render = FX::Render) noexcept
      : fx_timer(executor),           //
        fx_name{name},                //
        should_render{should_render}, //
        finished{false},              //
        next_fx{next_fx}              //
  {
    ensure_units();
  }

  virtual ~FX() noexcept { cancel(); };

  /// @brief Cancel any pending fx_timer actions
  void cancel() noexcept {
    [[maybe_unused]] error_code ec;
    fx_timer.cancel(ec);
  }

  /// @brief Is the FX complete as determined by the subclass
  /// @return boolean indicating the FX is complete
  virtual bool completed() const noexcept { return finished; }

  /// @brief Match the FX to a single name
  /// @param n Name to match
  /// @return boolean indicating if the single name matched
  bool match_name(csv n) const noexcept { return n == name(); }

  /// @brief The FX name
  /// @return contant string view of the name
  csv name() const noexcept { return fx_name; };

  /// @brief FX subclasses override to execute when FX called for the first time
  /// @return boolean, always true
  virtual bool once() noexcept { return true; }

  /// @brief translate Peaks into unit actions
  /// @param peaks Peaks to consider
  /// @param msg DataMsg to update (will be sent to ruth)
  /// @return boolean indicating if the FX is finished
  bool render(const Peaks &peaks, DataMsg &msg) noexcept;

  /// @brief The next FX suggested by external configuration file
  /// @return name, as a string, of the suggested FX
  const string &suggested_fx_next() const noexcept { return next_fx; }

  /// @brief Select the next FX
  /// @param strand strand for silence timer
  /// @param active_fx current active FX
  /// @param frame Frame to use for silence and can render
  static void select(strand_ioc &strand, std::unique_ptr<FX> &active_fx, Frame &frame) noexcept;

  /// @brief Get raw pointer to Unit subclass
  /// @tparam T Unit subclass type
  /// @param name Unit name
  /// @return Raw pointer to Unit subclass
  template <typename T = Unit> T *unit(const auto name) noexcept { return units->ptr<T>(name); }

protected:
  /// @brief Execute the FX subclass for audio peaks for a single frame
  /// @param peaks The audio peaks to use for FX execution
  virtual void execute(const Peaks &peaks) noexcept;

  /// @brief Set the FX as complete
  /// @param is_finished Boolean previous finished status of the FX
  /// @return previous finished value
  bool set_finished(bool is_finished, const string finished_fx = string()) noexcept {

    if (is_finished && !finished_fx.empty()) next_fx = finished_fx;

    return std::exchange(finished, is_finished);
  }

  /// @brief Adjust the silence timeout
  /// @param tokc conf::tokc pointer (assumes token has watch)
  /// @param silence_fx Name of FX to engage when timer expires
  /// @param def_val Deault timeout value if not found in token
  /// @return true if previous timeout did not match token timeout
  bool set_silence_timeout(conf::token *tokc, const string &silence_fx,
                           const auto &&def_val) noexcept {

    auto timeout = tokc->timeout_val("silence"_tpath, std::forward<decltype(def_val)>(def_val));

    return save_silence_timeout(timeout, silence_fx);
  }

  /// @brief Initiate silence watch timer
  /// @param silence_fx Name of FX to engage when timer expires
  void silence_watch(const string silence_fx = string()) noexcept;

private:
  /// @brief Create Units (call once)
  static void ensure_units() noexcept;

  /// @brief Save the timeout (likely new) and restart silence timer
  /// @param timeout
  /// @param silence_fx
  /// @return
  bool save_silence_timeout(const Millis &timeout, const string &silence_fx) noexcept;

protected:
  /// @brief Order dependent class member data
  static std::unique_ptr<Units> units;
  steady_timer fx_timer;
  const string fx_name;
  bool should_render;
  bool finished;

  // order independent
  Millis silence_timeout{0};
  string next_fx;

private:
  bool called_once{false};

public:
  MOD_ID("fx");
};

} // namespace desk
} // namespace pierre
