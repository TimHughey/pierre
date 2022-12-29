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

#include "logger.hpp"

namespace pierre {

std::shared_ptr<Logger> Logger::self;
Elapsed Logger::elapsed_runtime; // class static data

Logger::Logger(io_context &io_ctx) noexcept // static
    : io_ctx(io_ctx),                       // borrowed io_ctx
      local_strand(io_ctx)                  // serialize log msgs
{}

void Logger::init(io_context &io_ctx) noexcept { // static
  self = std::shared_ptr<Logger>(new Logger(io_ctx));
}

void Logger::teardown() noexcept {      // static
  auto s = self->ptr();                 // get a fresh shared_ptr to ourself
  auto &local_strand = s->local_strand; // get a reference to our strand, we'll move s

  self.reset(); // reset our static shared_ptr to ourself

  asio::post(local_strand, [s = std::move(s)]() {
    // keeps Logger in-scope until lambda completes
    // future implementation
  });
}

Logger::millis_fp Logger::runtime() noexcept { // static
  return std::chrono::duration_cast<millis_fp>((Nanos)elapsed_runtime);
}

} // namespace pierre
