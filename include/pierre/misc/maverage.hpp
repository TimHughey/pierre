/*
    misc/maverage.hpp -- Moving Average
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

#ifndef pierre_moving_average_hpp
#define pierre_moving_average_hpp

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

#include "local/types.hpp"

namespace pierre {

template <typename T> class MovingAverage {
  typedef std::deque<T> maDeque_t;
  using condv = std::condition_variable;
  using mutex = std::mutex;
  using unique_lock = std::unique_lock<mutex>;
  using seconds = std::chrono::seconds;
  using thread = std::thread;

public:
  MovingAverage(uint fill_seconds = 1.0) : _seconds(fill_seconds) {
    fillThreadStart();

    while (_fill_thread.joinable() == false) {
      std::this_thread::yield();
    }
  };

  void addValue(T val) {
    if (_filled) {
      // we now know the total number of items that represent the requested
      // duration (assuming frequency of adding values is a constant).
      // instead of adding more values we remove the oldest (at the front) and
      // add the new (to the back)
      _dq.pop_front();
      _dq.emplace_back(val);
    } else {

      if (_filling == false) { // fill timer needs to be started
        unique_lock lck(_filling_mtx);
        _filling = true;

        // adding the first value, notify the waiting fill timer thread
        _filling_cv.notify_all();
      }

      // add to container
      _dq.emplace_back(val);
    }
  }

  T latest() const { return calculate(); }
  T lastValue() const {
    T val = 0;
    if (_dq.empty() == false) {
      val = _dq.back();
    }
    return val;
  }

  size_t size() const { return _dq.size(); }

private:
  T calculate() const {
    T sum = 0;

    for (const T &val : _dq) {
      sum += val;
    }

    return sum / (T)_dq.size();
  }

  void fillThread() {
    while (_filling == false) { // wait for filling to start
      unique_lock lck(_filling_mtx);
      _filling_cv.wait(lck);
    }

    // local mtx, lock and cond var used as a timer
    mutex timer_mtx;
    unique_lock timer_lck(timer_mtx);
    condv initial_fill_cv;
    initial_fill_cv.wait_for(timer_lck, seconds(_seconds));

    _filled = true;
  }

  void fillThreadStart() { _fill_thread = thread(fillThreadStatic, this); }
  static void fillThreadStatic(MovingAverage<T> *obj) { obj->fillThread(); }

private:
  seconds _seconds = 1.0;
  bool _filling = false;
  bool _filled = false;

  mutex _filling_mtx;
  condv _filling_cv;

  thread _fill_thread;

  maDeque_t _dq = {};
};

} // namespace pierre
#endif
