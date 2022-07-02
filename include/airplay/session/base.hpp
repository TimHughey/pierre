//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#include "base/typical.hpp"
#include "common/ss_inject.hpp"
#include "io/io.hpp"

#include <memory>
#include <unordered_map>
#include <utility>

namespace pierre {
namespace airplay {
namespace session {

class Base;
typedef std::shared_ptr<Base> shBase;

class Base {
protected:
  enum ACCUMULATE : uint8_t { RX = 31, TX };

private:
  static constexpr csv DEF_SESSION_ID{"unknown session"};

public:
  Base(const Inject &di, csv session_id = DEF_SESSION_ID);

  virtual ~Base();

  virtual void asyncLoop() = 0;

  bool isReady() const { return socket.is_open(); };
  bool isReady(const error_code &ec);

  csv sessionId() const { return session_id; }
  virtual void shutdown() { teardown(); }
  virtual void teardown();

  void accumulate(ACCUMULATE type, size_t bytes) { _acc[type] += bytes; }
  const auto accumulated(ACCUMULATE type) { return _acc[type]; }

protected:
  // order dependent - initialized by constructor
  tcp_socket socket;
  strand local_strand;

private:
  string session_id; // used for logging

  std::unordered_map<ACCUMULATE, uint64_t> _acc;
};

} // namespace session
} // namespace airplay
} // namespace pierre
