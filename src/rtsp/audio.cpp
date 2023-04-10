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

#include "audio.hpp"
#include "base/types.hpp"
#include "desk/desk.hpp"
#include "frame/frame.hpp"
#include "frame/racked.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "rtsp/audio_full_packet.hpp"

#include <arpa/inet.h>
#include <boost/asio/append.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <ranges>
#include <span>
#include <vector>

namespace pierre {
namespace rtsp {

// static constexpr ssize_t PACKET_LEN_BYTES{sizeof(uint16_t)};

Audio::Audio(strand_ioc &local_strand, Ctx *ctx) noexcept
    : ctx(ctx),             // handle Audio for this rtsp CTX
      streambuf(16 * 1024), // allow buffering of sixteen packets
      acceptor{local_strand, tcp_endpoint{ip_tcp::v4(), ANY_PORT}},
      sock(local_strand, ip_tcp::v4()) // overwritten by accepting socket
{
  async_accept();
}

void Audio::async_read() noexcept {
  static constexpr csv fn_id{"async_read"};

  asio::async_read_until(
      sock, streambuf, audio::full_packet(), [this](const error_code &ec, size_t n) {
        if (!ec && n) {
          // happy path: no error and we have packet data

          // note:
          //  n represents the entire packet which includes the
          //  prefix (uint16_t) describing the audio data length.
          //  to copy only the audio data we must remove the PREFIX

          // pre allocate the raw audio data buffer
          uint8v raw_audio(n - audio::full_packet::PREFIX, 0x00);

          // use cheap asio buffers to copy the packet data (minus the prefix)
          const auto src_buff = asio::buffer(streambuf.data() + audio::full_packet::PREFIX);
          asio::buffer_copy(asio::buffer(raw_audio), src_buff);

          // consume the full packet
          streambuf.consume(n);

          // send the audio data for further processing (decipher, decode, etc)
          ctx->desk->handoff(std::move(raw_audio), ctx->shared_key);
        } else if (ec) {
          // error path
          INFO_AUTO("[falling through] n={} err={}\n", n, ec.what());
          return; // fall through
        } else {
          INFO_AUTO("SHORT READ, n={}\n", n);
        }

        async_read(); // TODO: this may not be a wise choice on short reads
      });
}

} // namespace rtsp
} // namespace pierre
