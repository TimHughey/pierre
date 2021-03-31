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

#ifndef pierre_lightdesk_fx_base_hpp
#define pierre_lightdesk_fx_base_hpp

#include <map>

#include "audio/peaks.hpp"
#include "lightdesk/headunits/tracker.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {
namespace fx {

class Fx {
public:
  Fx() = default;
  virtual ~Fx() = default;

  virtual void begin(){};
  virtual void execute(const audio::spPeaks peaks) = 0;
  virtual bool finished() { return _finished; }

  const std::map<audio::Freq_t, size_t> histogram() { return _histo; }

  void leave() { _tracker->leave(); }

  bool matchName(const string_t match) {
    auto rc = false;
    if (name().compare(match) == 0) {
      rc = true;
    }

    return rc;
  }

  virtual const string_t &name() const = 0;
  static void resetTracker() { _tracker.reset(); }
  static void setTracker(std::shared_ptr<HeadUnitTracker> tracker) {
    _tracker = std::move(tracker);
  }

  template <typename T> std::shared_ptr<T> unit(const string_t name) {
    auto x = _tracker->unit<T>(name);

    return std::move(x);
  }

protected:
  bool _finished = false;
  std::mutex _histo_mtx;
  std::map<audio::Freq_t, size_t> _histo;

private:
  static spHeadUnitTracker _tracker;
};

} // namespace fx
} // namespace lightdesk
} // namespace pierre

#endif
