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

#include <array>

#include <memory>

namespace pierre {
namespace desk {

class FX;

using fx_ptr = std::unique_ptr<FX>;

class FX {

public:
  static auto constexpr NoRender{false};
  static auto constexpr Render{true};

public:
  /// @brief Construct the base FX via the subclassed type
  FX(const string name, const string next_fx = fx::NONE, bool should_render = FX::Render) noexcept
      : // set the name of the subclass
        fx_name{name},
        // set if this FX should render or not
        should_render{should_render},
        // ensure finished is false at creation
        finished{false},
        // save the next FX to initiate when the silence timeout expires
        next_fx{next_fx} {
    ensure_units();
  }

  virtual ~FX() noexcept;

  /// @brief Is the FX complete as determined by the subclass
  /// @return boolean indicating the FX is complete
  virtual bool completed() const noexcept { return finished; }

  /// @brief Match the FX to a single name
  /// @param n Name to match
  /// @return boolean indicating if the single name matched
  bool match_name(auto n) const noexcept { return n == name(); }

  /// @brief The FX name
  /// @return contant string view of the name
  const auto &name() const noexcept { return fx_name; };

  /// @brief The next fx when completed
  /// @return name, as a string, of the suggested FX
  bool next(const string &match_next) const noexcept { return match_next == next_fx; }

  /// @brief FX subclasses override to execute when FX called for the first time
  /// @return boolean, always true
  virtual bool once() noexcept { return true; }

  /// @brief translate Peaks into unit actions
  /// @param peaks Peaks to consider
  /// @param msg DataMsg to update (will be sent to ruth)
  /// @return boolean indicating if the FX is finished
  bool render(const Peaks &peaks, DataMsg &msg) noexcept;

  /// @brief Select the next FX
  /// @param silence_timer timer to detect rendering of silence
  /// @param fx current active FX
  /// @param frame Frame to use for silence and can render
  static void select(fx_ptr &fx, Frame &frame) noexcept;

  bool silence_timeout() const noexcept { return frames[SilentCount] > frames[SilentMax]; }

  /// @brief Get raw pointer to Unit subclass
  /// @tparam T Unit subclass type
  /// @param name Unit name
  /// @return Raw pointer to Unit subclass
  template <typename T = Unit> T *unit(const auto name) noexcept { return units->ptr<T>(name); }

protected:
  /// @brief Execute the FX subclass for audio peaks for a single frame
  /// @param peaks The audio peaks to use for FX execution
  virtual void execute(const Peaks &peaks) noexcept;

  /// @brief Save silence timeout
  /// @param timeout X-reference to Millis
  /// @return boolean, when different true, otherwise false
  bool save_silence_timeout(Millis timeout) noexcept;

private:
  /// @brief Create Units (call once)
  static void ensure_units() noexcept;

protected:
  /// @brief Order dependent class member data
  static std::unique_ptr<Units> units;
  string fx_name;
  bool should_render;
  bool finished;

  // order independent
  string next_fx;

  // silence timeout frame count
  enum silenct_frame_t : uint8_t { SilentMax = 0, SilentCount, SilentEnd };
  std::array<int64_t, SilentEnd> frames{0};

private:
  bool called_once{false};

public:
  MOD_ID("fx");
};

} // namespace desk
} // namespace pierre
