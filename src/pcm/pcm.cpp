
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
#include <thread>

#include "decouple/conn_info.hpp"
#include "packet/basic.hpp"
#include "packet/rfc3550/hdr.hpp"
#include "packet/rfc3550/trl.hpp"
#include "pcm/pcm.hpp"
#include <anchor/anchor.hpp>

namespace pierre {

using namespace std::chrono_literals;
using namespace std::string_view_literals;

PulseCodeMod::PulseCodeMod(const Opts &opts)
    : queued(opts.audio_raw), stream_info(opts.stream_info) {
  thread = std::jthread([this]() { runLoop(); });

  //  give this thread a name
  pthread_setname_np(thread.native_handle(), "PCM");
}

void PulseCodeMod::runLoop() {
  std::once_flag first_packet;
  running = true;

  while (running) {
    // request next packet, wait for 1s
    auto next_packet = queued.nextPacket();

    next_packet.wait();
    auto packet = next_packet.get();

    // we have a packet!
    [[maybe_unused]] auto hdr = packet::rfc3550::hdr(packet);
    [[maybe_unused]] auto trl = packet::rfc3550::trl(packet);

    std::array<uint8_t, 16 * 1024> m;

    auto &shk = ConnInfo::inst()->sessionKey();

    if (shk.size() == 0) {
      if (false) { // debug
        constexpr auto f = FMT_STRING("{} no session key!\n");
        fmt::print(f, fnName());
      }
      continue;
    }

    // https://libsodium.gitbook.io/doc/secret-key_cryptography/aead/chacha20-poly1305/ietf_chacha20-poly1305_construction
    // Note: the eight-byte nonce must be front-padded out to 12 bytes.

    ullong_t decip_len = 0;
    const size_t cip_len = packet.size() - (8 + 12);
    auto cipher_rc = crypto_aead_chacha20poly1305_ietf_decrypt(
        m.data() + 7,       // m
        &decip_len,         // mlen_p
        nullptr,            // nsec,
        packet.data() + 12, // the ciphertext starts 12 bytes in and is followed by the MAC tag
        cip_len,            // clen -- the last 8 bytes are the nonce
        packet.data() + 4,  // authenticated additional data
        8,                  // authenticated additional data length
        trl.noncePtr(),     // the nonce
        shk.data());        // *k

    if (cipher_rc != 0) { //.yes, the cyrto function returns zero (0) for success
      constexpr auto f = FMT_STRING("{} bytes={}  seq_num={:>8}  ts={:>12}\n");
      fmt::print(f, fnName(), cip_len, decip_len, hdr.seqNum32(), hdr.timestamp());
    }

    if (false && (cipher_rc == 0)) { // debug
      std::vector<uint8_t> plain_data;
      auto to = std::back_inserter(plain_data);
      std::ranges::copy(m.begin(), m.begin() + decip_len, to);

      constexpr auto f = FMT_STRING("{} seq_num={:>18} local_ts={}\n");

      // const auto now = anchor::Clock::now();

      if (Anchor::use().use_count() > 1) {
        const auto local_ts = Anchor::use()->frameTimeToLocalTime(hdr.timestamp());
        // const int64_t diff = now - local_ts;

        fmt::print(f, fnName(), hdr.seqNum32(), local_ts);
      }
    }

    // trl.dump();
    std::this_thread::sleep_for(100ms);
  }

  auto self = pthread_self();

  std::array<char, 24> thread_name{0};
  pthread_getname_np(self, thread_name.data(), thread_name.size());
  fmt::print("{} work is complete\n", thread_name.data());
}

sPulseCodeMod PulseCodeMod::_instance_; // declaration of singleton instance

} // namespace pierre