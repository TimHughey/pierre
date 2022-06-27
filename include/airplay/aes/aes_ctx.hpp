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

#include "core/pair/pair.h"
#include "packet/content.hpp"
#include "packet/headers.hpp"
#include "packet/in.hpp"
#include "packet/out.hpp"
#include "packet/resp_code.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace pierre {
namespace airplay {

typedef struct pair_setup_context *PairCtx;
typedef struct pair_verify_context *VerifyCtx;
typedef struct pair_cipher_context *CipherCtx;
typedef struct pair_result *PairResult;

struct AesResult {
  bool ok = true;
  packet::RespCode resp_code = packet::RespCode::OK;
};

// NOTE: this struct consolidates the pairing "state"
class AesCtx {
public:
  AesCtx(const char *device_str);

  size_t decrypt(packet::In &packet, packet::In &ciphered);
  size_t encrypt(packet::Out &packet);

  // size_t decrypt()

  inline bool haveSharedSecret() const { return secretBytes() > 0; }
  AesResult verify(const packet::Content &in, packet::Content &out);

  inline auto secret() { return _result->shared_secret; }
  inline size_t secretBytes() const { return _result->shared_secret_len; }

  AesResult setup(const packet::Content &in, packet::Content &out);

private:
  inline CipherCtx cipher() { return _cipher; }

  packet::Content &copyBodyTo(packet::Content &out, const uint8_t *data, size_t bytes) const;

private:
  bool _decrypt_in = false;
  bool _encrypt_out = false;
  PairCtx _setup = nullptr;
  PairResult _result = nullptr;
  VerifyCtx _verify = nullptr;
  CipherCtx _cipher = nullptr;

  char *_pin = nullptr;

  static constexpr pair_type _pair_type = PAIR_SERVER_HOMEKIT;
};

} // namespace airplay
} // namespace pierre