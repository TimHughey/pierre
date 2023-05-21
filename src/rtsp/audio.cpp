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

#include "rtsp/audio.hpp"
#include "base/logger.hpp"
#include "base/types.hpp"
#include "rtsp/audio/packet.hpp"

#include <arpa/inet.h>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <vector>

namespace pierre {
namespace rtsp {

// static constexpr ssize_t PACKET_LEN_BYTES{sizeof(uint16_t)};

Audio::Audio(Ctx *ctx) noexcept
    : // handle Audio for this rtsp CTX
      ctx(ctx),
      // serialize audio data recv, processing
      strand(asio::make_strand(ctx->io_ctx.get_executor())),
      // allow buffering of 8k
      streambuf(8 * 1024),
      // create out acceptor
      acceptor{strand, tcp_endpoint{ip_tcp::v4(), ANY_PORT}},
      // create our socket
      sock(strand, ip_tcp::v4()) // overwritten by accepting socket
{
  INFO_INIT("sizeof={:>5}", sizeof(Audio));
  tcp_socket::enable_connection_aborted opt(true);
  acceptor.set_option(opt);

  // add work before starting thread and io_context
  async_accept();
}

void Audio::async_read() noexcept {
  static constexpr csv fn_id{"async_read"};

  asio::async_read_until(sock, streambuf, audio::packet(), [this](const error_code &ec, size_t n) {
    if (!ec && n) {
      // happy path: no error and we have packet data

      // note:
      //  n represents the entire packet which includes the
      //  prefix (uint16_t) describing the audio data length.
      //  to copy only the audio data we must remove the PREFIX

      // pre allocate the raw audio data buffer
      uint8v raw_audio(n - audio::packet::PREFIX, 0x00);

      // use cheap asio buffers to copy the packet data (minus the prefix)
      const auto src_buff = asio::buffer(streambuf.data() + audio::packet::PREFIX);
      asio::buffer_copy(asio::buffer(raw_audio), src_buff);

      // consume the full packet
      streambuf.consume(n);

      // send the audio data for further processing (decipher, decode, etc)
      ctx->audio_handoff(std::move(raw_audio));

      async_read(); // TODO: this may not be a wise choice on short reads
    } else if (ec) {
      // error path
      INFO_AUTO("[falling through] n={} msg={}\n", n, ec.message());
    } else {
      INFO_AUTO("SHORT READ, n={}\n", n);
    }
  });
}

} // namespace rtsp
} // namespace pierre
