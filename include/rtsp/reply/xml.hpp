/*
Pierre - Custom audio processing for light shows at Wiss Landing
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
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pierre {
namespace rtsp {

enum XMLDocs { InfoStage1 = 0 };

class XML {
public:
  static const char *doc(XMLDocs doc);
  static const size_t len(XMLDocs doc);

  static const char *binDoc(XMLDocs doc);
  static const size_t binLen(XMLDocs doc);
};

} // namespace rtsp
} // namespace pierre