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

#include <string>
#include <tuple>

#include "rtsp/content.hpp"

namespace pierre {
namespace rtsp {

typedef const std::string &cmethod;
typedef const std::string &cpath;

namespace reply {

class Final {
public:
  using Content = pierre::rtsp::Content;

public:
  Final(const auto final_tuple, const Content &content) : _content(content) {
    const auto &[ok, method, path] = final_tuple;

    _method = method;
    _path = path;
  }

  // public API
  const Content &content() const { return _content; }
  cmethod method() const { return _method; }
  cpath path() const { return _path; }

private:
  using string = std::string;

  string _method;
  string _path;
  const Content &_content;
};

} // namespace reply
} // namespace rtsp
} // namespace pierre