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
#include "lcs/config.hpp"
#include "rtsp/aplist.hpp"
#include "rtsp/reply.hpp"
#include "rtsp/request.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>

namespace pierre {
namespace rtsp {

class Saver {
private:
  // early decl due to auto
  void format_content(const Headers &headers, const uint8v &content, auto &w) const noexcept {

    if (headers.contains(hdr_type::ContentType)) {
      const auto type = headers.val(hdr_type::ContentType);

      if (type == hdr_val::AppleBinPlist) {
        Aplist(content).format_to(w);

      } else if (type == hdr_val::TextParameters) {
        std::copy_n(content.begin(), content.size(), w);

      } else if (type == hdr_val::OctetStream) {
        const auto len = headers.val<int64_t>(hdr_type::ContentLength);
        fmt::format_to(w, "<<OCTET STREAM LENGTH={}>>", len);
      }

      fmt::format_to(w, "{}{}", separator, separator);
    }
  }

public:
  Saver() noexcept : enable(config()->at("info.rtsp.saver.enable"sv).value_or(false)) {}

  template <typename T> void format_and_write(const T &r) noexcept {

    if (enable) {
      using U = std::remove_cvref_t<T>;

      uint8v buff;
      auto w = std::back_inserter(buff);

      if constexpr (std::is_same_v<U, Request>) {
        fmt::format_to(w, "{} {} RTSP/1.0{}", r.headers.method(), r.headers.path(), separator);
      } else if constexpr (std::is_same_v<U, Reply>) {
        fmt::format_to(w, "RTSP/1.0 {}{}", r.resp_code(), separator);
      }

      r.headers.format_to(w);
      fmt::format_to(w, "{}", separator);
      format_content(r.headers, r.content, w);

      write(buff);
    }
  }

  void write(const uint8v &buff) noexcept {
    auto cfg = Config::ptr();
    const auto base = cfg->at("info.rtsp.saver.path"sv).value_or(string("/tmp"));
    const auto file = cfg->at("info.rtsp.saver.file"sv).value_or(string("rtsp.log"));

    namespace fs = std::filesystem;
    fs::path path{fs::current_path()};
    path.append(base);

    try {
      fs::create_directories(path);

      fs::path full_path(path);
      full_path.append(file);

      std::ofstream out(full_path, std::ios::app);

      out.write(buff.raw<char>(), buff.size());

    } catch (const std::exception &e) {
      msg.assign(e.what());
    }
  }

private:
  bool enable{false};
  static constexpr csv separator{"\r\n"};

public:
  string msg;
  static constexpr csv module_id{"rtsp.saver"};
};

} // namespace rtsp

} // namespace pierre
