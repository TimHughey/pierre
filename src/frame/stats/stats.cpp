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

#include "frame/stats/stats.hpp"
#include "base/input_info.hpp"
#include "base/logger.hpp"
#include "config/config.hpp"

#include <InfluxDBFactory.h>
#include <memory>

namespace pierre {
namespace frame {

Stats::Stats(io_context &io_ctx, csv measure, const string db_uri) noexcept
    : db_uri(db_uri),       // where we save stats
      measurement(measure), // measurement
      stats_strand(io_ctx), // isolated strand for stats activities
      val_txt({             // create map of stats val to text
               {FLUSH_ELAPSED, "flush_elapsed"},
               {RACK_COLLISION, "rack_collision"},
               {REELS_FLUSHED, "reels_flushed"},
               {REELS_RACKED, "reels_racked"}}) //
{}

} // namespace frame
} // namespace pierre