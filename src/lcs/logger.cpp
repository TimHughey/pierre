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

#include "lcs/logger.hpp"
#include "lcs/config.hpp"

namespace pierre {

std::shared_ptr<Logger> Logger::self;
Elapsed Logger::elapsed_runtime; // class static data

bool Logger::should_log_info(csv mod, csv cat) noexcept { // static
  // in .cpp to avoid pulling config.hpp into Logger
  return cfg_info(mod, cat);
}

} // namespace pierre
