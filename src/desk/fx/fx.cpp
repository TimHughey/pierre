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

FX::~FX() noexcept {}

void FX::ensure_units() noexcept {
  if (!units) units = std::make_unique<Units>();
}

void FX::execute(const Peaks &) noexcept {}

bool FX::render(const Peaks &peaks, DataMsg &msg) noexcept {

  if (!should_render) return !finished;

  if (peaks.silence()) {
    frames[SilentCount] += 1;

    if (frames[SilentCount] > frames[SilentMax]) finished = true;
  } else {
    frames[SilentCount] = 0;
  }

  if (called_once == false) {
    called_once = once(); // frame 0 consumed by call to once(), peaks not rendere

  } else {
    units->prepare(); // do any prep required to render next frame
    execute(peaks);   // render frame into data msg
  }

  units->update_msg(msg); // populate data msg

  return !finished;
}

bool FX::save_silence_timeout(Millis timeout) noexcept {
  INFO_AUTO_CAT("set_silence");

  auto n = InputInfo::frame_count(timeout);

  auto rc{n != frames[SilentMax] || (frames[SilentMax] == 0)};

  if (rc) {
    INFO_AUTO("fx={} timeout={} frames={}", fx_name, dura::humanize(timeout), n);

    frames[SilentMax] = n;
  }

  return rc;
}

void FX::select(fx_ptr &fx, Frame &frame) noexcept {
  INFO_AUTO_CAT("select");

  if (!(bool)fx || fx->completed()) {
    auto fx_now = (bool)fx ? fx->name() : fx::NONE;

    // when fx::Standby is finished initiate standby()
    if (fx_now == fx::NONE) {
      // default to Standby
      fx = std::make_unique<desk::Standby>();

    } else if (frame.silent()) {
      // handle when the frame is silent

      if (fx->next(fx::STANDBY)) {
        fx = std::make_unique<desk::Standby>();
      } else if (fx->next(fx::ALL_STOP)) {
        fx = std::make_unique<desk::AllStop>();
      }

    } else if (!frame.silent() && frame.can_render()) {
      fx = std::make_unique<desk::MajorPeak>();
    }

    // note in log selected FX, if needed
    if (!fx->match_name(fx_now)) INFO_AUTO("FX {} -> {}\n", fx_now, fx->name());
  }
}

} // namespace desk
} // namespace pierre
