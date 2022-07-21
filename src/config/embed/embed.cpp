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

#include <memory>

// embedded binary data via ld (see cfg_fallback/CMakeLists.txt)
extern uint8_t _binary_fallback_json_start[];
extern uint8_t _binary_fallback_json_end;
extern uint8_t _binary_fallback_json_size;

namespace pierre {
namespace cfg {

csv fallback() {
  const char *begin = (const char *)&_binary_fallback_json_start;
  const char *end = (const char *)&_binary_fallback_json_end;

  return csv(begin, end - begin);
}

} // namespace cfg
} // namespace pierre
