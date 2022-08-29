/*
    Pierre - Custom Light Show for Wiss Landing
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

#include "desk/session/data.hpp"
#include "ArduinoJson.hpp"
#include "base/uint8v.hpp"
#include "desk/data_msg.hpp"
#include "io/async_msg.hpp"
#include "io/io.hpp"

#include <future>
#include <optional>

namespace pierre {
namespace desk {

void Data::log_connected() {
  __LOG0(LCOL01 " {}:{} -> {}:{} established, handle={}\n", moduleID(), "CONNECT",
         _socket->remote_endpoint().address().to_string(), _socket->remote_endpoint().port(),
         _socket->local_endpoint().address().to_string(), _socket->local_endpoint().port(),
         _socket->native_handle());
}

} // namespace desk
} // namespace pierre
