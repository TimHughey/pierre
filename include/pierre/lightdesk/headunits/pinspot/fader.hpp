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

#include "lightdesk/headunits/pinspot/color.hpp"
#include "local/types.hpp"

namespace pierre {
namespace lightdesk {

typedef class Fader Fader_t;
typedef class FaderOpts FaderOpts_t;

class FaderOpts {
public:
  FaderOpts() = default;

  FaderOpts(const Color_t &dest, float travel_secs)
      : dest(dest), travel_secs(travel_secs){};

  FaderOpts(const Color_t &dest, float ts, bool use_origin, float a = 1.0,
            float l = 0.0)
      : dest(dest), travel_secs(ts), use_origin(use_origin), accel(a),
        layover(l){};

  FaderOpts(const Color_t &origin, const Color_t &dest, float ts = 1.0,
            bool use_origin = false, float a = 1.0, float l = 0.0)
      : origin(origin), dest(dest), travel_secs(ts), use_origin(use_origin),
        accel(a), layover(l){};

public:
  Color_t origin;
  Color_t dest;
  float travel_secs = 1.0;
  bool use_origin = false;
  float accel = 0.0;
  float layover = 0.0;
};

class Fader {
public:
  Fader(){};

  inline bool active() const { return !_finished; }
  inline bool finished() const { return _finished; }

  inline const FaderOpts_t &initialOpts() const { return _opts; }

  const Color_t &location() const { return _location; }
  void print() {
    printf("loc[%3.2f %3.2f %3.2f %3.2f]\n"
           "vel[%3.2f %3.2f %3.2f %3.2f]\n"
           "dir[%3.2f %3.2f %3.2f %3.2f]\n",
           _location.colorPartConst(RED_PART),
           _location.colorPartConst(GREEN_PART),
           _location.colorPartConst(BLUE_PART),
           _location.colorPartConst(WHITE_PART), _velocity.velocity(RED_PART),
           _velocity.velocity(GREEN_PART), _velocity.velocity(BLUE_PART),
           _velocity.velocity(WHITE_PART), _velocity.direction(RED_PART),
           _velocity.direction(GREEN_PART), _velocity.direction(BLUE_PART),
           _velocity.direction(WHITE_PART));
  }

  void prepare(const FaderOpts_t &opts) {
    _finished = false;
    _traveled = false;
    _opts = opts;

    _location = _opts.origin;

    _velocity.calculate(_opts.origin, _opts.dest, _opts.travel_secs);
  }

  void prepare(const Color_t &origin, FaderOpts_t opts) {
    opts.origin = origin;
    prepare(opts);
  }

  bool travel() {
    bool more_travel = false;

    if ((_traveled == false) && (_opts.use_origin)) {
      // when use_origin is set start the first travel is to the origin
      // so _location is unchanged
      more_travel = true;
    } else {
      // if we've already traveled once then we move from _location
      _velocity.moveColor(_location, _opts.dest, more_travel);

      _finished = !more_travel;
    }

    _traveled = true;

    return more_travel;
  }

private:
  FaderOpts_t _opts;
  Color_t _location;         // current fader location
  ColorVelocity_t _velocity; // velocity required to travel to destination

  bool _traveled = false;
  bool _finished = true;

  float _acceleration = 0.0;
};

} // namespace lightdesk
} // namespace pierre

#endif
