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

#include "headers.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <iterator>
#include <regex>
#include <set>
#include <span>
#include <sstream>

using namespace std::literals::string_view_literals;

namespace pierre {

static constexpr auto re_syntax{std::regex_constants::ECMAScript};

bool Headers::parse(uint8v &packet, const uint8v::delims_t &delims) noexcept {

  if (parse_ok) return parse_ok;

  // create stream using whatever we have based on the delim at idx
  // note: first == start of delim, second == len of delim
  // note: delims[0]: method, delims[1]: header block
  const auto end_pos = delims[1].first + delims[1].second; // end of block to parse
  string backing_store(packet.raw(), end_pos);             // backing store for in_stream
  std::istringstream in_stream(backing_store);             // parsing method and header block

  // parse the method, path and protocol if we haven't already
  if (_method.empty()) {
    // build regex once
    // note: regex matches and ignores the CR
    static std::regex re_method{"([A-Z_]+) (.+) (RTSP/1.0)\\r", re_syntax}; // build this once

    // Example Method
    //
    // POST /fp-setup RTSP/1.0<CR><LF>

    // NOTE: index 0 is entire string
    constexpr auto method_idx{1};
    constexpr auto path_idx{2};
    constexpr auto protocol_idx{3};
    constexpr auto matches{4}; // four matches including whole string

    std::smatch sm;

    string line;                              // get_line() makes a copy
    if (std::getline(in_stream, line)) {      // line is string up to LF (includeing the CR)
      std::regex_search(line, sm, re_method); // note: regex matches and ignores the CR in the line

      if (sm.ready() && (sm.size() == matches)) {
        _method = sm[method_idx].str();
        _path = sm[path_idx].str();
        _protocol = sm[protocol_idx].str();
      }
    } else {
      // we don't reparse this line so ignore everything up to the first LF
      const auto max = delims[0].first + delims[0].second;
      in_stream.ignore(max, '\n');
    }
  }

  // parse the header block if delim is available

  static std::regex re_header_pair{"^([A-Za-z-]+): (.{1,})?\\r", re_syntax}; // build this once

  // Example Header
  //
  // Content-Type: application/octet-stream
  // Content-Length: 16
  // User-Agent: Music/1.2.2 (Macintosh; OS X 12.2.1) AppleWebKit/612.4.9.1.8
  // Client-Instance: BAFE421337BA1913<CR><LF><CR><LF>

  // NOTE: index 0 is entire string
  constexpr auto type_idx{1};
  constexpr auto val_idx{2};
  constexpr size_t matches{3}; // three matches including whole string

  for (string line; std::getline(in_stream, line);) {
    // line is string up to LF (includeing the CR)

    if (line.size() > 1) {
      std::smatch sm;
      std::regex_search(line, sm, re_header_pair); // regex matches and ignores the CR

      if (sm.ready() && (sm.size() == matches)) {
        add(sm[type_idx].str(), sm[val_idx].str());
        parse_err.clear();
      } else {
        parse_err = fmt::format("ready={} matches={} '{}'\n", sm.ready(), sm.size(), sm[0].str());
      }
    }
  }

  parse_ok = true;

  return parse_ok;
}

// initialize static data
const string hdr_type::AppleHKP{"Apple-HKP"};
const string hdr_type::AppleProtocolVersion{"Apple-ProtocolVersion"};
const string hdr_type::ContentLength{"Content-Length"};
const string hdr_type::ContentSimple{"Content"};
const string hdr_type::ContentType{"Content-Type"};
const string hdr_type::CSeq{"CSeq"};
const string hdr_type::DacpActiveRemote{"Active-Remote"};
const string hdr_type::DacpID{"DACP-ID"};
const string hdr_type::Public{"Public"};
const string hdr_type::RtpInfo{"RTP-Info"};
const string hdr_type::Server{"Server"};
const string hdr_type::UserAgent{"User-Agent"};
const string hdr_type::XAppleAbsoulteTime{"X-Apple-AbsoluteTime"};
const string hdr_type::XAppleClientName{"X-Apple-Client-Name"};
const string hdr_type::XAppleET{"X-Apple-ET"};
const string hdr_type::XAppleHKP{"X-Apple-HKP"};
const string hdr_type::XApplePD{"X-Apple-PD"};
const string hdr_type::XAppleProtocolVersion{"X-Apple-ProtocolVersion"};

const string hdr_val::AirPierre{"AirPierre/366.0"};
const string hdr_val::AppleBinPlist{"application/x-apple-binary-plist"};
const string hdr_val::ConnectionClosed{"close"};
const string hdr_val::ImagePng{"image/png"};
const string hdr_val::OctetStream{"application/octet-stream"};
const string hdr_val::TextParameters{"text/parameters"};

std::set<string> Headers::known_types{
    // for easy sorting via IDE
    hdr_type::AppleHKP,
    hdr_type::AppleProtocolVersion,
    hdr_type::ContentLength,
    hdr_type::ContentSimple,
    hdr_type::ContentType,
    hdr_type::CSeq,
    hdr_type::DacpActiveRemote,
    hdr_type::DacpID,
    hdr_type::Public,
    hdr_type::RtpInfo,
    hdr_type::Server,
    hdr_type::UserAgent,
    hdr_type::XAppleAbsoulteTime,
    hdr_type::XAppleClientName,
    hdr_type::XAppleET,
    hdr_type::XAppleHKP,
    hdr_type::XApplePD,
    hdr_type::XAppleProtocolVersion,
    //
};

} // namespace pierre