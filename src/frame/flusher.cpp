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

#include "frame/flusher.hpp"
#include "base/logger.hpp"

#include <cassert>
#include <exception>
#include <set>

namespace pierre {

void Flusher::accept(FlushInfo &&fi_next) noexcept {
  INFO_AUTO_CAT("accept");

  if (fi_next.no_accept()) assert("invalid kind for accept()");

  INFO_AUTO(">> {}", fi_next);

  // protect copying of new flush details
  sema.acquire();

  fi = std::move(fi_next);

  INFO_AUTO("== {}", fi);
}

void Flusher::done() noexcept {
  INFO_AUTO_CAT("done");

  fi.done();
  INFO_AUTO("{}", fi);

  sema.release();
}

} // namespace pierre