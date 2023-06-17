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

    The Easings functions are copied from and/or inspired by:
    https:://www.wasings.net  (Andrey Sitnik and Ivan Solovev)
*/

#pragma once

#include "base/types.hpp"

#include <cmath>
#include <numbers>
#include <variant>

namespace pierre {
namespace desk {
namespace fader {

template <typename T>
concept IsEasing = requires(T v) {
  { v.step } -> std::same_as<double>;
  { v.start } -> std::same_as<double>;

  requires requires(T v, double current, double total) {
    { v(current, total) } -> std::same_as<double>;
  };
};

struct easing {
  virtual double operator()(double progress) const noexcept = 0;

  static constexpr auto PI{std::numbers::pi};
  static constexpr auto PI_HALF{PI / 2.0};
};

struct out_expo : public easing {
  double operator()(double x) const noexcept override final {
    return x == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * x);
  }
};

} // namespace fader
} // namespace desk

/*

struct CircularAcceleratingFromZero : public Easing {
  constexpr CircularAcceleratingFromZero() = default;
  constexpr CircularAcceleratingFromZero(double step = 1.0f, double start_val = 0.0f)
      : Easing(step, start_val) {}

  double calc(double current, double total) const;
};

struct CircularDeceleratingToZero : public Easing {
  CircularDeceleratingToZero(double step = 1.0f, double start_val = 0.0f)
      : Easing(step, start_val) {}

  double calc(double current, double total) const;
};

struct Quadratic : public Easing {
  Quadratic(double step = 1.0f, double start_val = 0.0f) : Easing(step, start_val) {}

  double calc(double current, double total) const;
};

struct QuintAcceleratingFromZero : public Easing {
  QuintAcceleratingFromZero(double step = 1.0f, double start_val = 0.0f)
      : Easing(step, start_val) {}

  double calc(double current, double total) const;
};

struct QuintDeceleratingToZero : public Easing {
  QuintDeceleratingToZero(double step = 1.0f, double start_val = 0.0f) : Easing(step, start_val) {}

  double calc(double current, double total) const;
};*/

/*struct Sine : public Easing {
  Sine(double step = 1.0f, double start_val = 0.0f) : Easing(step, start_val) {}

  double calc(double current, double total) const;
};

struct SineAcceleratingFromZero : public Easing {
  SineAcceleratingFromZero(double step = 1.0f, double start_val = 0.0f) : Easing(step, start_val) {}

  double calc(double current, double total) const;
};

struct SineDeceleratingToZero : public Easing {
  SineDeceleratingToZero(double step = 1.0f, double start_val = 0.0f) : Easing(step, start_val) {}

  double calc(double current, double total) const;
};*/

} // namespace pierre
