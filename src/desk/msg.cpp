// Pierre
// Copyright (C) 2022  Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "desk/msg.hpp"

namespace pierre {
namespace desk {

// misc logging, debug
string Msg::inspect() const noexcept {
  string msg;

  fmt::format_to(std::back_inserter(msg), "packed_len={}\n", measureMsgPack(doc));

  serializeJsonPretty(doc, msg);

  return msg;
}

// error_code Msg::log_rx(const error_code ec, const size_t bytes, const char *err) noexcept {
//   if (ec || (packed_len != bytes) || err) {
//     INFO(module_id, "LOG_RX", "failed, bytes={}/{} reason={} deserialize={}\n", bytes, tx_len,
//          ec.message(), err);
//   }

//   return ec;
// }

// error_code Msg::log_tx(const error_code ec, const size_t bytes) noexcept {
//   if (ec || (tx_len != bytes)) {
//     if (ec != errc::operation_canceled) {
//       INFO(module_id, "LOG_TX", "bytes={}/{} {}\n", bytes, tx_len, ec.message());
//     }
//   }

//   return ec;
// }

} // namespace desk
} // namespace pierre