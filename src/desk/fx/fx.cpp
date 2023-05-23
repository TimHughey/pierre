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
#include "base/dura.hpp"
#include "base/logger.hpp"
#include "desk/fx/all.hpp"
#include "desk/msg/data.hpp"
#include "desk/unit/all.hpp"
#include "desk/unit/names.hpp"
#include "frame/frame.hpp"
#include "frame/peaks.hpp"

namespace pierre {
namespace desk {

std::unique_ptr<Units> FX::units{nullptr}; // headunits available for FX (static class data)

void FX::ensure_units() noexcept {
  if (!units) units = std::make_unique<Units>();
}

void FX::execute(const Peaks &peaks) noexcept { peaks.noop(); };

bool FX::render(const Peaks &peaks, DataMsg &msg) noexcept {

  if (!should_render) return !finished;

  if (called_once == false) {
    called_once = once(); // frame 0 consumed by call to once(), peaks not rendere

  } else {
    units->prepare(); // do any prep required to render next frame
    execute(peaks);   // render frame into data msg
  }

  units->update_msg(msg); // populate data msg

  return !finished;
}

bool FX::save_silence_timeout(const Millis &timeout, const string &silence_fx) noexcept {
  INFO_AUTO_CAT("set_silence");

  if (timeout != std::exchange(silence_timeout, timeout)) {
    INFO_AUTO("silence_timeout={}", dura::humanize(silence_timeout));

    silence_watch(silence_fx);
    return true;
  }

  return false;
}

void FX::select(strand_ioc &strand, std::unique_ptr<FX> &active_fx, Frame &frame) noexcept {
  INFO_AUTO_CAT("select");

  if (!(bool)active_fx || active_fx->completed()) {
    // cache multiple use frame info
    const auto silent = frame.silent();

    const auto fx_now = active_fx ? active_fx->name() : fx::NONE;
    const auto fx_next = active_fx ? active_fx->suggested_fx_next() : fx::STANDBY;

    // when fx::Standby is finished initiate standby()
    if (fx_now == fx::NONE) { // default to Standby
      active_fx = std::make_unique<desk::Standby>(strand);
    } else if (silent && (fx_next == fx::STANDBY)) {
      active_fx = std::make_unique<desk::Standby>(strand);

    } else if (!silent && (frame.can_render())) {
      active_fx = std::make_unique<desk::MajorPeak>(strand);

    } else if (silent && (fx_next == fx::ALL_STOP)) {
      active_fx = std::make_unique<desk::AllStop>(strand);
    } else {
      active_fx = std::make_unique<desk::Standby>(strand);
    }

    // note in log selected FX, if needed
    if (!active_fx->match_name(fx_now)) INFO_AUTO("FX {} -> {}\n", fx_now, active_fx->name());
  }
}

void FX::silence_watch(const string silence_fx) noexcept {
  INFO_AUTO_CAT("silence_watch");

  if (silence_fx.size() && (silence_fx != next_fx)) {
    INFO_AUTO("next_fx new={} old={}", silence_fx, std::exchange(next_fx, silence_fx));
  }

  fx_timer.expires_after(silence_timeout);
  fx_timer.async_wait([this](const error_code &ec) {
    if (ec) return;

    set_finished(true);
    INFO_AUTO("timer fired, finished={} next_fx={}", finished, next_fx);
  });
}

} // namespace desk
} // namespace pierre
