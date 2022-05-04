
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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#include <chrono>
#include <fmt/format.h>
#include <sodium.h>
#include <string_view>

#include "decouple/conn_info.hpp"
#include "packet/basic.hpp"
#include "packet/rfc3550/hdr.hpp"
#include "packet/rfc3550/trl.hpp"
#include "pcm/pcm.hpp"

namespace pierre {

using namespace std::chrono_literals;
using namespace std::string_view_literals;

PulseCodeMod::PulseCodeMod(const Opts &opts)
    : queued(opts.audio_raw), nptp(opts.nptp), stream_info(opts.stream_info) {
  thread = std::jthread([this]() { runLoop(); });

  //  give this thread a name
  const auto handle = thread.native_handle();
  pthread_setname_np(handle, "PCM");
}

void PulseCodeMod::runLoop() {
  running = true;

  while (running) {
    packet::Basic packet;

    if (queued.waitForPacket()) {
      auto packet = queued.nextPacket();

      [[maybe_unused]] auto hdr = packet::rfc3550::hdr(packet);
      [[maybe_unused]] auto trl = packet::rfc3550::trl(packet);

      if (true) {
        std::array<uint8_t, 16 * 1024> m;

        // https://libsodium.gitbook.io/doc/secret-key_cryptography/aead/chacha20-poly1305/ietf_chacha20-poly1305_construction
        // Note: the eight-byte nonce must be front-padded out to 12 bytes.
        ullong_t deciphered_len = 0;
        auto &shk = ConnInfo::inst()->sessionKey();
        auto cipher_rc = crypto_aead_chacha20poly1305_ietf_decrypt(
            m.data() + 7,             // m
            &deciphered_len,          // mlen_p
            nullptr,                  // nsec,
            packet.data() + 12,       // the ciphertext starts 12 bytes in and is followed
                                      // by the MAC tag,
            packet.size() - (8 + 12), // clen -- the last 8 bytes are the nonce
            packet.data() + 4,        // authenticated additional data
            8,                        // authenticated additional data length
            trl.noncePtr(),           // the nonce
            shk.data());              // *k

        if (cipher_rc) {
          constexpr auto f = FMT_STRING("{} bytes={}  seq_num={:>8}  ts={>:12}\n");
          fmt::print(f, fnName(), hdr.seqNum32(), hdr.timestamp());
        }

        // trl.dump();
        std::this_thread::sleep_for(100ms);
      }

    } else {
      std::this_thread::sleep_for(250ms);
    }
  }

  auto self = pthread_self();

  std::array<char, 24> thread_name{0};
  pthread_getname_np(self, thread_name.data(), thread_name.size());
  fmt::print("{} work is complete\n", thread_name.data());
}

sPulseCodeMod PulseCodeMod::_instance_; // declaration of singleton instance

} // namespace pierre