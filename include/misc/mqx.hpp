/*
    pierre.hpp - Audio Transmission
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

#include <chrono>
#include <condition_variable> // std::condition_variable
#include <memory>
#include <mutex> // std::mutex, std::unique_lock
#include <queue>
#include <vector>

namespace pierre {

typedef std::mutex Mutex_t;
typedef std::unique_lock<Mutex_t> XLock_t;
typedef std::lock_guard<Mutex_t> LockGuard_t;
typedef std::condition_variable PendingBuffer_t;

template <typename T> class MsgQX {

public:
  MsgQX(size_t max_depth = 10) : _max_depth(max_depth) {}

  size_t discards() {
    auto discards = _discards;

    _discards = 0;

    return discards;
  }

  void maxDepth(size_t depth) { _max_depth = depth; }

  T latest() {

    std::lock_guard lck(_latest_mtx);

    T item = _latest;

    return (item);
  }

  T pop() {

    T item;

    {
      std::unique_lock lck(_mtx);

      _available.wait(lck, [this] { return _queue.empty() == false; });

      item = _queue.front();
      _queue.pop();
    }

    return (item);
  }

  T pop(const long ms, bool &timeout) {
    timeout = false;
    std::chrono::milliseconds wait_ms(ms);
    T item;

    {
      std::unique_lock lck(_mtx);

      if (_available.wait_for(lck, wait_ms, [this] { return _queue.empty() == false; })) {
        item = _queue.front();
        _queue.pop();
      } else {
        timeout = true;
      }
    }

    return (item);
  }

  void push(T item) {
    {
      std::lock_guard latest_lck(_latest_mtx);
      _latest = item;
    }

    {
      std::lock_guard lck(_mtx);

      if (_queue.size() == _max_depth) {
        _queue.pop();
        _discards++;
      }

      _queue.push(std::move(item));
    }

    _available.notify_one();
  }

  std::mutex &mutex() { return _mtx; }
  std::queue<T> &queue() { return _queue; }

private:
  size_t _max_depth;

  std::mutex _mtx;
  std::queue<T> _queue;
  std::condition_variable _available;

  std::mutex _latest_mtx;
  T _latest;

  size_t _discards = 0;
};
} // namespace pierre
