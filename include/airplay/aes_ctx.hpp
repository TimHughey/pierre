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

#include "base/content.hpp"
#include "base/headers.hpp"
#include "base/resp_code.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "pair/pair.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace pierre {

struct AesResult {
  bool ok{true};
  RespCode resp_code{RespCode::OK};
};

// NOTE: this struct consolidates the pairing "state"
class AesCtx {
public:
  AesCtx(csv device_str);

  size_t decrypt(uint8v &packet, uint8v &ciphered);
  size_t encrypt(uint8v &packet);

  // size_t decrypt()

  inline bool have_shared_secret() const { return secret_bytes() > 0; }
  AesResult verify(const Content &in, Content &out);

  inline auto secret() { return result->shared_secret; }
  inline size_t secret_bytes() const { return result->shared_secret_len; }

  AesResult setup(const Content &in, Content &out);

private:
  auto cipher() { return cipher_ctx; }

  Content &copy_body_to(Content &out, const uint8_t *data, size_t bytes) const;

private:
  bool decrypt_in{false};
  bool encrypt_out{false};

  pair_cipher_context *cipher_ctx{nullptr};
  pair_result *result{nullptr};
  pair_setup_context *setup_ctx{nullptr};
  pair_verify_context *verify_ctx{nullptr};
};

} // namespace pierre