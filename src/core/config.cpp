/*
    Pierre - Custom Light Show for Wiss Landing
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

#include <filesystem>

#define TOML_IMPLEMENTATION

#include "core/config.hpp"
#include "external/toml.hpp"

using namespace std;

namespace fs = std::filesystem;

namespace pierre {
namespace core {

const error_code &Config::exists() const { return _exists_rc; }

const string_view Config::fallback() const {
  auto raw = R"(
    [pierre]
    title = "Pierre Live Config"
    version = 1
    working_dir = "/tmp/pierre"
    log_dir = "/tmp/pierre"

    [dmx]
    host = "test-with-devs.ruth"
    port = "48005"

    [dsp]
    fft = {samples = 1024, rate = 48000}
    logging = {file = "dsp.log", enable = false, truncate = true}

    [lightdesk]
    colorbars = false

    [lightdesk.majorpeak.logging]
    file = "majorpeak.log"
    truncate = true
    peaks = false
    colors = false

    [lightdesk.majorpeak.color]
    reference = { hue = 0.0, sat = 100.0, bri = 100.0}
    random_start = false
    rotate = {enable = false, ms = 7000}

    [lightdesk.majorpeak.frequencies]
    hard = {ceiling = 10000.0, floor = 40.0}
    soft = {ceiling = 1500.0, floor = 110.0}

    [lightdesk.majorpeak.pinspot.fill]
    name = 'fill'
    frequency_max = 1000.0
    fade_max_ms = 800

    [lightdesk.majorpeak.pinspot.fill.when_greater]
    frequency = 180.0
    brightness_min = 3.0
    higher_frequency = {brightness_min = 80.0}

    [lightdesk.majorpeak.pinspot.fill.when_lessthan]
    frequency = 180.0
    brightness_min = 27.0

    [lightdesk.majorpeak.pinspot.main]
    name = 'main'
    fade_max_ms = 700
    frequency_min = 180.0
    when_fading = {brightness_min = 5.0, freq_greater = {brightness_min = 69.0}}

    [lightdesk.majorpeak.makecolor.above_soft_ceiling]
    hue = {min = 345.0, max = 355.0, step = 0.0001}
    brightness = {max = 50.0, mag_scaled = true}

    [lightdesk.majorpeak.makecolor.generic]
    hue = {min = 30.0, max = 360.0, step = 0.0001}
    brightness = {max = 100.0, mag_scaled = true}

    [pcm.logging]
    file = "pcm.log"
    init = false
    truncate = true

    [pcm.alsa]
    device = 'hw:CARD=sndrpihifiberry,DEV=0'
    format = "S16_LE"
    channels = 2
    rate = 48000
    avail_min = 128
  )"sv;

  return move(raw);
}

auto Config::operator[](const std::string_view &subtable) {
  return move(_tbl[subtable]);
}

const error_code &Config::parse(path &file, bool use_embedded) {
  _file = file;

  if (fs::exists(_file, _exists_rc)) {
    _tbl = toml::parse_file(_file.c_str());
  } else if (use_embedded) {
    _tbl = toml::parse(fallback());
  }

  return _exists_rc;
}

} // namespace core
} // namespace pierre
