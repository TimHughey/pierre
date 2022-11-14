
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

#include "base/accum.hpp"
#include "base/elapsed.hpp"
#include "base/io.hpp"
#include "base/logger.hpp"
#include "base/pet.hpp"
#include "base/types.hpp"

#include <InfluxDBFactory.h>
#include <concepts>
#include <map>
#include <memory>
#include <type_traits>

namespace pierre {
namespace frame {

enum stats_v { //
  FLUSH_ELAPSED = 0,
  RACK_COLLISION,
  RACK_WIP_TIMEOUT,
  REELS_FLUSHED,
  REELS_RACKED
};

class Stats : public std::enable_shared_from_this<Stats> {
private:
  Stats(io_context &io_ctx, csv measure, const string db_uri) noexcept;

public:
  static auto init(io_context &io_ctx, csv measure, const string db_uri) noexcept {
    return std::shared_ptr<Stats>(new Stats(io_ctx, measure, db_uri))->init_db();
  }

  void write(stats_v vt, auto v) noexcept {

    asio::post(stats_strand, //
               [=, this]() { //
                 auto pt = influxdb::Point(module_id.data());

                 if constexpr (std::is_same_v<decltype(v), double> ||
                               std::is_same_v<decltype(v), float>) {

                   pt.addField("val", v).addTag("type", "float");
                 } else if constexpr (std::is_same_v<decltype(v), Elapsed>) {
                   pt.addField("val", v().count).addTag("type", "duration");
                 } else if constexpr (std::is_same_v<decltype(v), bool>) {
                   pt.addField("val", v == true ? 1 : 0).addTag("type", "boolean");
                 } else {
                   pt.addField("val", v).addTag("type", "misc");
                 }

                 pt.addTag("metric", val_txt[vt]);

                 db->write(std::move(pt));
               });
  }

private:
  std::shared_ptr<Stats> init_db() noexcept {
    db = influxdb::InfluxDBFactory::Get(db_uri);
    db->batchOf();

    return shared_from_this();
  }

private:
  // order dependent
  const string db_uri;
  const string measurement;
  strand stats_strand;
  std::map<stats_v, string> val_txt;

private:
  // order independent
  std::unique_ptr<influxdb::InfluxDB> db;

public:
  static constexpr csv module_id{"FRAME_STATS"};
};

} // namespace frame
} // namespace pierre