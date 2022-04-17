/*
    Pierre - Custom Light Show for Wiss Landing
    Copyright (C) 2022  Tim Hughey

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

#include <algorithm>
#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <string>

#include "rtsp/aes_ctx.hpp"
#include "rtsp/packet_in.hpp"
#include "rtsp/reply/packet_out.hpp"

using namespace std;

namespace pierre {
namespace rtsp {

AesCtx::AesCtx(const char *device_str) {
  // allocate the setup and verify contexts
  _setup = pair_setup_new(_pair_type, _pin, NULL, NULL, device_str);

  if (_setup == nullptr) {
    static const char *msg = "pair_setup_new() failed";
    fmt::print("{}\n", msg);
    throw(runtime_error(msg));
  }

  _verify = pair_verify_new(_pair_type, NULL, NULL, NULL, device_str);
  if (_verify == nullptr) {
    static const char *msg = "pair_verify_new() failed";
    fmt::print("{}\n", msg);
    throw(runtime_error(msg));
  }
}

Content &AesCtx::copyBodyTo(Content &out, const uint8_t *body,
                            size_t bytes) const {
  out.clear();

  if (body && (bytes > 0)) {
    out.reserve(bytes); // reserve() NOT resize()
    auto where = std::back_inserter(out);
    auto last_byte = body + bytes;

    std::copy(body, last_byte, where);

    delete body;
  }

  return out;
}

// this function is a NOP until cipher is established and the first encrypted
// packet is received
size_t AesCtx::encrypt(rtsp::PacketOut &packet) {
  ssize_t bytes_ciphered = packet.size();

  if (cipher() && _encrypt_out) {
    uint8_t *ciphered_data;
    size_t ciphered_len;

    auto rc = pair_encrypt(&ciphered_data, &ciphered_len, packet.data(),
                           packet.size(), _cipher);

    auto __ciphered_data = make_unique<uint8_t *>(ciphered_data);

    if (rc >= 0) { // success
      packet.reset();

      // NOTE: PacketOUt is based on a vector so we can use std::copy()
      auto where = std::back_inserter(packet);
      auto last_byte = ciphered_data + ciphered_len;

      std::copy(ciphered_data, last_byte, where);
      bytes_ciphered = ciphered_len;

    } else {
      auto msg = string(pair_cipher_errmsg(_cipher));
      fmt::print("encryption failed: {}\n", msg);
    }
  }

  return bytes_ciphered;
}

size_t AesCtx::decrypt(PacketIn &packet, size_t bytes) {
  size_t bytes_consumed = bytes;

  if (cipher()) {
    uint8_t *plain_data = nullptr;
    size_t plain_len = 0;
    const uint8_t *cipher_data = (uint8_t *)packet.data();

    bytes_consumed =
        pair_decrypt(&plain_data, &plain_len, cipher_data, bytes, cipher());
    // when we've decrypted an inbound packet we should always encrypt outbound
    _encrypt_out = true;

    //   fmt::print("decrypted bytes={}\n", bytes_consumed);

    if (bytes_consumed > 0) {
      uint8_t *replace_packet = (uint8_t *)packet.data();
      // copy the plain text back to the inbound packet
      // NOTE: PacketIn is based on array so we must do the copy manually
      for (size_t idx = 0; idx < bytes_consumed; idx++) {
        replace_packet[idx] = plain_data[idx];
      }
    }

    delete plain_data;
  }

  return bytes_consumed;
}

AesResult AesCtx::verify(const Content &in, Content &out) {
  AesResult aes_result;
  uint8_t *body = nullptr;
  size_t body_bytes = 0;

  if (pair_verify(&body, &body_bytes, _verify, in.data(), in.size()) < 0) {
    aes_result.resp_code = AuthRequired;
    copyBodyTo(out, body, body_bytes);
    return aes_result;
  }

  if ((pair_verify_result(&_result, _verify) == 0) && haveSharedSecret()) {
    _cipher = pair_cipher_new(_pair_type, 2, secret(), secretBytes());

    if (_cipher != nullptr) {
      // enable inbound encryption, the next packet will be encrypted
      _decrypt_in = true;
    } else {
      fmt::print("error setting up control channel ciper\n");
      aes_result.resp_code = InternalServerError;
    }
  }

  copyBodyTo(out, body, body_bytes);
  return aes_result;
}

AesResult AesCtx::setup(const Content &in, Content &out) {
  AesResult aes_result;
  uint8_t *body = nullptr;
  size_t body_bytes = 0;

  // first time pair_setup is called it will not be able to find the state
  // return AuthRequired

  if (pair_setup(&body, &body_bytes, _setup, in.data(), in.size()) < 0) {
    aes_result.resp_code = RespCode::AuthRequired;
    copyBodyTo(out, body, body_bytes);
    return aes_result;
  }

  // if we made it here the setup context is in good shape, see if the result
  // is available (we have shared secret data)
  if ((pair_setup_result(NULL, &_result, _setup) == 0) && haveSharedSecret()) {
    // Transient pairing completed (pair-setup step 2), prepare encryption, but
    // don't activate yet, the response to this request is still plaintext
    _cipher = pair_cipher_new(_pair_type, 2, secret(), secretBytes());

    // NOTE: ciper may be null indicating setup is incomplete. we still return
    // the body from pair_setup() to complete the setup phase
  }

  copyBodyTo(out, body, body_bytes);
  return aes_result;
}

} // namespace rtsp
} // namespace pierre
