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

#include "airplay/aes_ctx.hpp"
#include "airplay/content.hpp"
#include "airplay/headers.hpp"
#include "airplay/resp_code.hpp"
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "reply/inject.hpp"

#include <array>
#include <cctype>
#include <fmt/core.h>
#include <memory>
#include <optional>
#include <ranges>
#include <tuple>
#include <vector>

namespace pierre {
namespace airplay {
namespace reply {

class Reply {

  // Building the response:
  // 1. Include CSeq header from request
  // 2. Include Server header
  // 3. Add Content
  // 4. Write data to socket

  // Payload Format:
  //
  // RTSP/1.0 200 OK\r\n
  // CSeq: <from request>
  // Content-Type: <based on content>
  // Header1: Value1\r\n
  // Header2: Value2\r\n
  // <more headers each followed by \r\n>
  // Content-Length: <val>  <<-- if there is content
  // \r\n  <<-- separate headers from content
  // <binary or plist content>

public:
  Reply() : module_id("REPLY") {}
  Reply(csv module_id) : module_id(module_id) {}

  csv baseID() const { return base_id; }
  uint8v &build();
  Reply &inject(const reply::Inject &di);

  /// @brief populates reply headers and content, pure virtual
  /// @return boolean indicating if success assembling reply
  virtual bool populate() = 0;

  void copyToContent(std::shared_ptr<uint8_t[]> data, const size_t bytes) {
    copyToContent(data.get(), bytes);
  }

  void copyToContent(const uint8_t *begin, const size_t bytes) {
    std::copy(begin, begin + bytes, std::back_inserter(_content));
  }

  void copyToContent(const auto &buf) noexcept {
    std::ranges::copy(buf, std::back_inserter(_content));
  }

  // access injected dependencies
  auto &aesCtx() { return di->aes_ctx; }
  auto &method() const { return di->method; }
  auto &path() const { return di->path; }
  const Content &plist() const { return di->content; }
  const Content &rContent() const { return di->content; }
  const Headers &rHeaders() const { return di->headers; };

  // direct access to injected dependencies
  const Inject &injected() { return *di; }

  virtual csv moduleID() const { return module_id; }

  inline void resp_code(RespCode code) { _rcode = code; }
  // inline string_view responseCodeView() const { return respCodeToView(_rcode); }

  // misc debug
  void dump() const;
  void log_reply(csv resp_text);

protected:
  // order dependent
  string_view module_id;

  std::optional<reply::Inject> di; // copy, ok because it's only references

  RespCode _rcode{RespCode::NotImplemented};
  Headers headers;

  Content _content;
  uint8v _packet;

  static constexpr csv base_id{"REPLY"};
};

} // namespace reply
} // namespace airplay
} // namespace pierre