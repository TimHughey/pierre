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

#include "rtsp/saver.hpp"
#include "base/config/toml.hpp"

#include <fmt/format.h>
#include <fmt/os.h>
#include <iterator>

namespace pierre {
namespace rtsp {

Saver::Saver(Saver::Direction direction, const Headers &headers, const uint8v &content,
             const RespCode resp_code) noexcept
    : ctoken(module_id) //
{

  if (ctoken.val<bool, toml::table>("enable"_tpath, false)) {
    static constexpr csv def_file{"/tmp/rtsp.log"};
    static constexpr csv def_format{"raw"};

    const string file{ctoken.val<string, toml::table>("file"_tpath, def_file.data())};
    const string format{ctoken.val<string, toml::table>("format"_tpath, def_format.data())};

    uint8v buff;
    auto w = std::back_inserter(buff);

    headers.format_to(w);

    fmt::format_to(w, "{}", separator);

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

    try {
      auto mode = fmt::file::WRONLY | fmt::file::CREATE | fmt::file::APPEND;
      auto out = fmt::output_file(file.c_str(), mode);

      if (direction == Saver::IN) {
        out.print("{} {} RTSP/1.0{}{}", headers.method(), headers.path(), separator, buff.view());
      } else {
        out.print("RTSP/1.0 {}{}{}", resp_code(), separator, buff.view());
      }
    } catch (const std::exception &e) {
      msg.assign(e.what());
    }
  }
}

} // namespace rtsp
} // namespace pierre
