/*
    devs/pinspot/fader.hpp - Ruth Pin Spot Fader Action
    Copyright (C) 2020  Tim Hughey

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

#ifndef pierre_pinspot_fader_hpp
#define pierre_pinspot_fader_hpp

#include <iostream>

#include "lightdesk/color.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

typedef class Fader Fader_t;
typedef class FaderOpts FaderOpts_t;

class FaderOpts {
public:
  FaderOpts() = default;

  FaderOpts(const Color &dest, float travel_secs)
      : dest(dest), travel_secs(travel_secs){};

  FaderOpts(const Color &dest, float ts, bool use_origin, float a = 1.0,
            float l = 0.0)
      : dest(dest), travel_secs(ts), use_origin(use_origin), accel(a),
        layover(l){};

  FaderOpts(const Color &origin, const Color &dest, float ts = 1.0,
            bool use_origin = false, float a = 1.0, float l = 0.0)
      : origin(origin), dest(dest), travel_secs(ts), use_origin(use_origin),
        accel(a), layover(l){};

public:
  Color origin;
  Color dest;
  float travel_secs = 1.0;
  bool use_origin = false;
  float accel = 0.0;
  float layover = 0.0;
};

class Fader {
public:
  Fader() = default;

  bool active() const { return !_finished; }
  bool checkProgress(double percent) const;
  bool finished() const { return _finished; }
  const FaderOpts_t &initialOpts() const { return _opts; }
  const Color &location() const { return _location; }
  void prepare(const FaderOpts_t &opts);
  void prepare(const Color &origin, FaderOpts_t opts);
  double progress() const { return 100.0 - (_progress * 100.0); }

  bool travel();

private:
  FaderOpts_t _opts;
  Color _location; // current fader location
  bool _traveled = false;
  bool _finished = true;

  float _acceleration = 0.0;

  uint _frames = 0;

  double _step = 0.0;
  double _progress = 0.0;
};

typedef std::shared_ptr<Fader> spFader;

} // namespace lightdesk
} // namespace pierre

#endif
