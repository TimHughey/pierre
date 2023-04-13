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

#include "base/types.hpp"
#include "desk/color.hpp"
#include "desk/fx.hpp"
#include "frame/peaks.hpp"
#include "lcs/config/token.hpp"

#include <memory>

namespace pierre {
namespace desk {

class Standby : public FX {

public:
  Standby(auto &executor) noexcept : FX(executor, fx::STANDBY), ctoken(module_id) {

    // handler to process config changes
    // notes:
    //  -build the handler here due to the deduced type of executor
    //  -move the handler to register_token() so the call to config2::watch
    //   is in the .cpp file
    auto handler = [&, this](std::any &&next_table) mutable {
      // for clarity build the post action separately then move into post
      auto action = asio::append(
          [this](std::any t) {
            ctoken.table = std::move(t);
            apply_config();
          },
          std::move(next_table));

      asio::post(executor, std::move(action));
    };

    // note: the table is initially copied to the token during registration
    register_token(std::move(handler)); // in .cpp to hide config2::watch

    apply_config();
  }

  void execute(const Peaks &peaks) noexcept override;

  void once() noexcept override final;

private:
  void apply_config() noexcept;
  void register_token(config2::token::lambda &&handler) noexcept;

private:
  // order depdendent
  config2::token ctoken;

  // order independent
  Color first_color;
  Color next_color;
  double hue_step{0.0};
  double max_brightness{0};
  double next_brightness{0};

public:
  static constexpr csv module_id{"fx.standby"};
};

} // namespace desk
} // namespace pierre
