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
#include <ctime>
#include <fmt/format.h>
#include <iostream>
#include <iterator>
#include <plist/plist.h>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <typeinfo>

#include <rtsp/request.hpp>

namespace pierre {
namespace rtsp {

using namespace std;

Request::Request(sAesCtx aes_ctx, sService service)
    : _aes_ctx(aes_ctx), _service(service) {}

void Request::dump(DumpKind dump_type) {
  std::time_t now = std::time(nullptr);
  std::string dt = std::ctime(&now);

  fmt::print("\n>>> {}", dt);

  if (dump_type == HeadersOnly) {
    fmt::print("\n{} bytes={:04} ", (_ok ? "OK" : "FAILURE"), _bytes);
    fmt::print("method={} path={} protocol={} ", _method, _path, _protocol);
    fmt::print("content_len={:03}\n", _content.size());

    headersDump();
  }

  if (dump_type == ContentOnly) {
    auto &content_type = headerGetValue(ContentType);

    if (content_type.compare("application/octet-stream") == 0) {
      fmt::print("\nContent bytes={:03}\n", _content.size());

      for (size_t byte_num = 0; const auto &byte : _content) {
        fmt::print("{:03}[0x{:02x}] ", byte_num, byte);

        byte_num++;

        if ((byte_num % 10) == 0) {
          fmt::print("\n");
        }
      }
    }
  }

  if (dump_type == RawOnly) {
    fmt::print("\nRAW\n---\n");
    auto out = string_view{(const char *)_packet.data(), _bytes};
    fmt::print("{:s}\n---\n", out);
  }

  fmt::print("\n");
}

auto Request::findPlist(const auto bol, const auto abs_end) const {
  constexpr auto *plist_hdr = "bplist00";
  constexpr size_t plist_hdr_len = sizeof(plist_hdr);

  for (auto it = bol; bol < abs_end; it++) {
    // short circuit search if char doesn't match first plist hdr byte
    if (*it != plist_hdr[0])
      continue;

    auto stop = it;
    std::advance(stop, plist_hdr_len);

    if (stop >= abs_end) {
      break;
    }

    // first byte matched, compare entire plist hdr
    if (strncmp(it, plist_hdr, plist_hdr_len) == 0) {
      return it;
    }
  }

  return abs_end;
}

bool Request::findSeparator() {
  // separator between headers and context
  constexpr const char *sep = "\r\n\r\n";
  constexpr const size_t sep_len = strlen(sep);

  // strstr() returns a pointer to the start of the seperator string
  auto sep_offset = strstr(_packet.begin(), sep);

  if (sep_offset == nullptr) {
    fmt::print("Request::findSeperator() could not find separator\n");
    return false;
  }

  // keep the final header block separator for regex to find them
  _header_bytes = std::distance(_packet.begin(), sep_offset) + (sep_len / 2);

  // calculate the content offset from the beginning of the packet
  _content_offset = std::distance(_packet.begin(), sep_offset) + sep_len;

  return true;
}

auto Request::importPlist(const auto plist_start, const auto plist_end) {
  const auto bytes = std::distance(plist_start, plist_end);
  plist_t info_plist = nullptr;

  plist_from_memory(plist_start, bytes, &info_plist);

  char *xml = nullptr;
  uint32_t len = 0;
  plist_to_xml(info_plist, &xml, &len);
  // plist_to_bin(info_plist, &plist_bin, &plist_len);

  fmt::print("plist\n{}", string(xml, len));

  // auto qual = plist_dict_get_item(info_plist, "qualifier");
  // auto array_size = plist_array_get_size(qual);

  // if (array_size < 1) {
  //   return;
  // }

  // auto array_val = plist_array_get_item(qual, 0);
  // char *val_cstr = nullptr;
  // plist_get_string_val(array_val, &val_cstr);

  // fmt::print("array[0]={}\n", val_cstr);

  plist_free(info_plist);
  free(xml);
}

// primary entry point for inbound packets
void Request::parse() {
  findSeparator();

  auto headers_begin = _packet.begin();
  auto headers_end = _packet.begin() + _header_bytes;

  for (auto bol = headers_begin; printable(*bol) && (bol < headers_end);) {
    // end of line sentinel
    constexpr auto CR = 0x0a;

    // lamba
    auto isCarriageReturn = [](const auto i) { return i == CR; };
    auto eol = find_if(bol, headers_end, isCarriageReturn);

    if (eol < headers_end) {
      const auto line = string{bol, eol};

      // find the method first
      if (_method.empty()) {
        parseMethod(line);
      } else {
        parseHeader(line);
      }
    }

    bol = ++eol;
  }

  // ensure no left-over data
  _content.clear();

  // if no content loaded and there is content_length, copy it
  if (shouldLoadContent()) {
    _content.reserve(_content_length);
    auto where = back_inserter(_content);

    auto begin = _packet.begin() + _content_offset;
    auto end = begin + _content_length;

    std::copy(begin, end, where);

    dump(ContentOnly);
  }
}

void Request::parseHeader(const string &line) {
  // Example Headers
  //
  // Content-Type: application/octet-stream
  // Content-Length: 16
  // User-Agent: Music/1.2.2 (Macintosh; OS X 12.2.1) AppleWebKit/612.4.9.1.8
  // Client-Instance: BAFE421337BA1913

  // NOTE: index 0 is entire string
  constexpr auto key_idx = 1;
  constexpr auto val_idx = 2;
  constexpr auto match_count = 3;

  // headers of note
  constexpr auto key_content_len = "Content-Length";

  smatch sm;
  auto re_method = regex{"([\\w-]*): (\\w[^\\r\\n]*)", re_syntax};
  regex_search(line, sm, re_method);

  _ok = sm.ready();

  if (_ok && (sm.size() == match_count)) {
    const auto &key = sm[key_idx].str();
    const auto &val = sm[val_idx].str();

    headerAdd(key, val);

    if (key.compare(key_content_len) == 0) {
      const auto len = atoi(val.c_str());

      // save the numeric value of content length
      _content_length = len;
    }
  }
}

void Request::parseMethod(const string &line) {
  using namespace std::literals;

  // Example Method
  //
  // POST /fp-setup RTSP/1.0

  // NOTE: index 0 is entire string
  constexpr auto method_idx = 1;
  constexpr auto path_idx = 2;
  constexpr auto protocol_idx = 3;
  constexpr auto match_count = 4;

  fmt::print("\t\t{}\n", line);

  smatch sm;
  // static auto re_method = regex{"([A-Z]*) (.[a-zA-Z-_]*) (.*)", re_syntax};
  static auto re_method = regex(R"(([A-Z]*) (.*) (RTSP.*))", re_syntax);
  regex_search(line, sm, re_method);

  _ok = sm.ready();

  if (_ok && (sm.size() == match_count)) {
    _method = sm[method_idx].str();
    _path = sm[path_idx].str();
    _protocol = sm[protocol_idx].str();
  }
}

bool Request::shouldLoadContent() { return ((_content_length != 0) && _content.empty()); }

} // namespace rtsp
} // namespace pierre