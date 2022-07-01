/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#pragma once

#include "base/typical.hpp"
#include "dsp/peaks.hpp"
#include "fx/names.hpp"
#include <desk/fx/histogram.hpp>

#include <memory>

namespace pierre {

class FX;
typedef std::shared_ptr<FX> shFX;

class FX {
public:
  FX() = default;
  virtual ~FX() = default;

  virtual bool completed() { return finished; }

  // signal to caller if once() was called by returning the stored valued
  void executeLoop(shPeaks peaks);

  const auto histogram() const { return histo.map; }
  bool matchName(csv &match) const { return match == name(); }
  virtual csv name() const = 0;
  virtual void once() {} // subclasses should override once() to run setup code one time

protected:
  virtual void execute(shPeaks peaks) = 0;

protected:
  bool finished = false;
  fx::Histogram histo;

private:
  bool called_once = false;
};

} // namespace pierre
