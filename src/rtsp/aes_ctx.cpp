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

#include "aes_ctx.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "base/uint8v.hpp"
#include "reply.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>

namespace pierre {

static constexpr pair_type HOMEKIT{PAIR_SERVER_HOMEKIT};

AesCtx::AesCtx() {
  const auto device_id = Host().device_id().data();

  // allocate the setup and verify contexts
  char *pin{nullptr};
  setup_ctx = pair_setup_new(HOMEKIT, pin, NULL, NULL, device_id);

  if (setup_ctx == nullptr) {
    static constexpr csv msg{"pair_setup_new() failed"};
    throw(std::runtime_error(msg.data()));
  }

  verify_ctx = pair_verify_new(HOMEKIT, NULL, NULL, NULL, device_id);
  if (verify_ctx == nullptr) {
    static constexpr csv msg{"pair_verify_new() failed"};
    throw(std::runtime_error(msg.data()));
  }
}

// this function is a NOP until cipher is established and the first encrypted
// packet is received
size_t AesCtx::encrypt(uint8v &packet) noexcept {
  ssize_t bytes_ciphered = packet.size();

  if (cipher_ctx && encrypt_out) {
    uint8_t *data;
    size_t ciphered_len;

    auto rc = pair_encrypt(&data, &ciphered_len, packet.data(), packet.size(), cipher_ctx);

    std::unique_ptr<uint8_t[]> xxx(data); // auto free the cipher buffer

    if (rc >= 0) { // success
      packet.clear();

      std::copy(data, data + ciphered_len, std::back_inserter(packet));
      bytes_ciphered = ciphered_len;
    } else {
      INFO(module_id, "ENCRYPT", "{}\n", pair_cipher_errmsg(cipher_ctx));
    }
  }

  return bytes_ciphered;
}

/// @brief decrypt a chunk of data once pairing is complete, passthrough otherwise
/// @param packet destination of decrypted (or passed through) data chunk
/// @param ciphered source data chunk
/// @return ciphered bytes consumed by decryption
ssize_t AesCtx::decrypt(uint8v &packet, uint8v &ciphered) noexcept {
  ssize_t consumed{0};

  if (cipher_ctx) {
    // when we've decrypted an inbound packet we should always encrypt outbound
    encrypt_out = true;

    // we have a cipher
    uint8_t *data = nullptr;
    size_t len = 0;

    consumed = pair_decrypt(&data, &len, ciphered.data(), ciphered.size(), cipher_ctx);
    std::unique_ptr<uint8_t> xxx(data); // auto free the data buffer

    // if any byte we're consumed we remove them from the ciphered buffer and append
    // them to the packet

    if (consumed > 0) {
      std::copy(data, data + len, std::back_inserter(packet));

      // create a new cipher packet of the unconsumed bytes
      uint8v cipher_rest;
      cipher_rest.assign(ciphered.begin() + consumed, ciphered.end());
      std::swap(ciphered, cipher_rest);
    }
  } else {
    //  plain text, just swap the data
    std::swap(ciphered, packet);
    consumed = std::ssize(packet);
  }

  return consumed;
}

AesResult AesCtx::verify(const Content &in, Content &out) noexcept {
  AesResult aes_result;
  uint8_t *body{nullptr};
  size_t body_bytes{0};

  if (pair_verify(&body, &body_bytes, verify_ctx, in.data(), in.size()) < 0) {
    aes_result.resp_code = RespCode::AuthRequired;

  } else if ((pair_verify_result(&result, verify_ctx) == 0) && have_shared_secret()) {
    cipher_ctx = pair_cipher_new(HOMEKIT, 2, result->shared_secret, result->shared_secret_len);

    if (cipher_ctx) {
      // enable inbound encryption, the next packet will be encrypted
      decrypt_in = true;
    } else {
      fmt::print("error setting up control channel ciper\n");
      aes_result.failed();
    }

  } else {
    aes_result.failed();
  }

  if (aes_result.ok) {
    copy_to(out, body, body_bytes);
  }
  return aes_result;
}

AesResult AesCtx::setup(const Content &in, Content &out) noexcept {
  AesResult aes_result;
  uint8_t *body{nullptr};
  size_t body_bytes{0};

  if (pair_setup(&body, &body_bytes, setup_ctx, in.data(), in.size()) < 0) {
    // this is the first pair-setup message since we failed setup, return
    // auth required (we'll get another pair-setup message)
    aes_result.resp_code = RespCode::AuthRequired;

  } else if ((pair_setup_result(NULL, &result, setup_ctx) == 0) && have_shared_secret()) {
    // transient pairing completed (pair-setup step 2), prepare encryption, but
    // don't activate yet, the response to this request is still plaintext
    cipher_ctx = pair_cipher_new(HOMEKIT, 2, result->shared_secret, result->shared_secret_len);
  }

  // NOTE:
  // ciper may be null indicating setup is incomplete.
  // return the body of the message to complete setup

  copy_to(out, body, body_bytes);

  return aes_result;
}

} // namespace pierre
