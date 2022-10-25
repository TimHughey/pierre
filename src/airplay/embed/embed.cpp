/*
    Pierre - Custom Light Show via DMX for Wiss Landing
    Copyright (C) 2021  Tim Hughey

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

#include "embed/embed.hpp"
#include "base/types.hpp"

// embedded binary data via ld (see ple/CMakeLists.txt)
extern uint8_t _binary_get_info_resp_plist_start[];
extern uint8_t _binary_get_info_resp_plist_end;
extern uint8_t _binary_get_info_resp_plist_size;

namespace pierre {
namespace airplay {

csv ple::binary(const ple::Embedded &embedded) { // static
  const char *begin = nullptr;
  const char *end = nullptr;

  switch (embedded) {
  case GetInfoRespStage1:
    begin = (const char *)&_binary_get_info_resp_plist_start;
    end = (const char *)&_binary_get_info_resp_plist_end;
    break;
  }

  return csv(begin, end - begin);
}

} // namespace airplay
} // namespace pierre

[[maybe_unused]] static bool __embedded = true;