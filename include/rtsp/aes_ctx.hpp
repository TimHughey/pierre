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
#include "base/uint8v.hpp"
#include "pair/pair.h"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace pierre {

/// @brief Consolidated view of pairing result including RTSP response code
struct AesResult {
  bool ok{true};
  RespCode resp_code{RespCode::OK};

  /// @brief Mark the result as failed by setting
  ///        RespCode::InternalServerError and ok==false
  void failed() noexcept {
    ok = false;
    resp_code = RespCode::InternalServerError;
  }
};

/// @brief Encapsulates RTSP encryption, decryption and pairing state
class AesCtx {
public:
  AesCtx();

  ~AesCtx() {
    pair_cipher_free(cipher_ctx);
    pair_setup_free(setup_ctx);
    pair_verify_free(verify_ctx);
  }

  /// @brief decrypt a chunk of data once pairing is complete otherwise passthrough
  /// @param request request containing ciphered data and deciphered destination
  /// @return ciphered bytes consumed by decryption
  ssize_t decrypt(rtsp::Request &request) noexcept;
  size_t encrypt(uint8v &packet) noexcept;

  AesResult setup(const uint8v &in, uint8v &out) noexcept;
  AesResult verify(const uint8v &in, uint8v &out) noexcept;

private:
  /// @brief Copies raw uint8_t array to container
  /// @param out destination container
  /// @param data pointer to raw uint8_t array
  /// @param bytes num of bytes to copy
  /// @return reference to destination container
  uint8v &copy_to(uint8v &out, uint8_t *data, ssize_t bytes) const {

    if (data && (bytes > 0)) {
      std::unique_ptr<uint8_t> xxx(data);

      out.clear();
      out.reserve(bytes); // reserve() NOT resize()

      std::copy(data, data + bytes, std::back_inserter(out));
    }

    return out;
  }

  bool have_shared_secret() const noexcept { return result->shared_secret_len > 0; }

private:
  bool decrypt_in{false};
  bool encrypt_out{false};

  pair_cipher_context *cipher_ctx{nullptr};
  pair_result *result{nullptr};
  pair_setup_context *setup_ctx{nullptr};
  pair_verify_context *verify_ctx{nullptr};

public:
  static constexpr csv module_id{"rtsp.aes_ctx"};
};

} // namespace pierre