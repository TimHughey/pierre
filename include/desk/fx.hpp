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

#include "base/pet.hpp"
#include "base/types.hpp"
#include "desk/msg/data.hpp"
#include "desk/units.hpp"
#include "frame/fdecls.hpp"
#include "fx/names.hpp"
#include "lcs/logger.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/system.hpp>
#include <chrono>
#include <initializer_list>
#include <memory>

namespace pierre {
namespace asio = boost::asio;
namespace sys = boost::system;
namespace errc = boost::system::errc;

using error_code = boost::system::error_code;
using steady_timer = asio::steady_timer;

namespace desk {

class FX {

public:
  static auto constexpr NoRender{false};
  static auto constexpr Render{true};

public:
  /// @brief Construct the base FX via the subclassed type
  FX(auto &executor, const string name, bool should_render = FX::Render)
  noexcept
      : fx_timer(executor), fx_name{name},
        should_render{should_render}, finished{false}, next_fx{fx::NONE} {
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
  virtual void once() noexcept {}

  /// @brief translate Peaks into unit actions
  /// @param peaks Peaks to consider
  /// @param msg DataMsg to update (will be sent to ruth)
  /// @return boolean indicating if the FX is finished
  bool render(const Peaks &peaks, DataMsg &msg) noexcept;

  /// @brief The next FX suggested by external configuration file
  /// @return name, as a string, of the suggested FX
  const string &suggested_fx_next() const noexcept { return next_fx; }

  /// @brief Will this FX render the audio peaks
  /// @return boolean, caller can use this flag to determine if upstream work is required

protected:
  /// @brief Execute the FX subclass for audio peaks for a single frame
  /// @param peaks The audio peaks to use for FX execution
  virtual void execute(const Peaks &peaks) noexcept;

  /// @brief Set the FX as complete
  /// @param is_finished Boolean indicating the finished status of the FX
  void set_finished(bool is_finished, const string finished_fx = string()) noexcept {

    if (is_finished && !finished_fx.empty()) next_fx = finished_fx;

    std::exchange(finished, is_finished);
  }

  template <typename T>
  bool set_silence_timeout(const T &new_val, const string silence_fx) noexcept {
    static constexpr csv fn_id{"set_silence"};
    using Millis = std::chrono::milliseconds;

    auto new_val_millis = pet::as<Millis, T>(new_val);

    if (silence_timeout != new_val_millis) {
      silence_timeout = new_val_millis;

      INFO_AUTO("new silence_timeout={}\n", pet::humanize(silence_timeout));

      silence_watch(silence_fx);
      return true;
    }

    return false;
  }

  void silence_watch(const string silence_fx = string()) noexcept {
    static constexpr csv fn_id{"silence_watch"};

    if (silence_fx.size() && (silence_fx != next_fx)) {
      INFO_AUTO("next_fx new={} old={}\n", silence_fx, std::exchange(next_fx, silence_fx));
    }

    fx_timer.expires_after(silence_timeout);
    fx_timer.async_wait([this](const error_code &ec) {
      if (ec) return;

      set_finished(true);
      INFO_AUTO("timer fired, finished={} next_fx={}\n", finished, next_fx);
    });
  }

private:
  static void ensure_units() noexcept;

protected:
  /// @brief Order dependent class member data
  static Units units;
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
  static constexpr csv module_id{"fx"};
  static constexpr csv cfg_silence_timeout{"silence.timeout.minutes"};
};

} // namespace desk
} // namespace pierre
