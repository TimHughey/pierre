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

#include "desk/fx.hpp"
#include "base/pet.hpp"
#include "desk/msg/data.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/names.hpp"
#include "frame/peaks.hpp"
#include "lcs/logger.hpp"

namespace pierre {
namespace desk {

Units FX::units; // headunits available for FX (static class data)

void FX::ensure_units() noexcept {
  if (units.empty()) units.create_all_from_cfg(); // create the units once
}

void FX::execute(const Peaks &peaks) noexcept { peaks.noop(); };

bool FX::render(const Peaks &peaks, DataMsg &msg) noexcept {
  if (should_render) {
    if (called_once == false) {
      once();                // frame 0 consumed by call to once(), peaks not rendered
      units.update_msg(msg); // ensure any once() actions are in the data msg

      called_once = true;

    } else {
      units.prepare();           // do any prep required to render next frame
      execute(std::move(peaks)); // render frame into data msg
      units.update_msg(msg);     // populate data msg
    }
  }

  return !finished;
}

bool FX::save_silence_timeout(const Millis &timeout, const string &silence_fx) noexcept {
  static constexpr csv fn_id{"set_silence"};

  if (timeout != std::exchange(silence_timeout, timeout)) {
    INFO_AUTO("silence_timeout={}\n", pet::humanize(silence_timeout));

    silence_watch(silence_fx);
    return true;
  }

  return false;
}

void FX::silence_watch(const string silence_fx) noexcept {
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

} // namespace desk
} // namespace pierre
