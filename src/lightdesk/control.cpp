/*
    lightdesk/control.cpp - Ruth Light Desk Control
    Copyright (C) 2020  Tim Hughey

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

#include "lightdesk/control.hpp"
#include "lightdesk/lightdesk.hpp"

namespace pierre {

using TR = reading::Text;

bool LightDeskControl::config() {
  I2s_t *i2s = _desk->i2s();
  auto rc = true;

  const float generic = i2s->dBFloor();
  const float bass = i2s->bassdBFloor();
  const float complexity = i2s->complexitydBFloor();

  printf("\ndB floor: generic(%10.1f) bass(%10.1f) complexity(%10.1f)\n\n",
         generic, bass, complexity);

  return rc;
}

bool LightDeskControl::handleCommand(MsgPayload_t &msg) {
  bool rc = false;
  StaticJsonDocument<256> doc;

  auto err = deserializeMsgPack(doc, msg.data());

  const auto root = doc.as<JsonObject>();

  if (err) {
    TR::rlog("LightDesk command parse failure: %s", err.c_str());
    return rc;
  }

  const auto mode_obj = root["mode"].as<JsonObject>();

  if (mode_obj) {
    const bool ready_flag = mode_obj["ready"];
    const bool stop_flag = mode_obj["stop"];
    const bool dance_flag = mode_obj["dance"];

    if (ready_flag) {
      rc = ready();

    } else if (dance_flag) {
      const auto dance_interval = mode_obj["opts"]["interval_secs"] | 23.3;

      rc = dance(dance_interval);

    } else if (stop_flag) {
      stop();
    }

    rc = true;
  }

  return rc;
}

bool LightDeskControl::objectSizes() {
  auto rc = true;

  printf("lightdesk(%5u) fx(%5u) dmx(%u) i2s(%5u) pinspot(%5u) control(%5u)\n",
         sizeof(LightDesk_t), sizeof(FxType_t), sizeof(Dmx_t), sizeof(I2s_t),
         sizeof(PinSpot_t), sizeof(LightDeskControl_t));

  printf("\n");

  return rc;
}

bool LightDeskControl::setMode(LightDeskMode_t mode) {
  _mode = mode;

  if (_desk == nullptr) {
    _desk = new LightDesk();
  }

  auto rc = _desk->request(_request);

  return rc;
}

bool LightDeskControl::stats() {
  auto rc = true;
  static const char *indent = "                    ";
  const size_t indent_size = strlen(indent);
  const LightDeskStats_t &stats = _desk->stats();
  const DmxStats_t &dmx = stats.dmx;
  const I2sStats_t &i2s = stats.i2s;
  const FxStats_t &fx = stats.fx;

  // lightdesk
  printf("\n");
  printf("%-*smode(%3s) ac_power(%3s)\n", indent_size, "lightdesk:", stats.mode,
         stats.ac_power ? "on" : "off");

  printf("%sel wire dance_floor(%4u) entry(%4u)\n", indent,
         stats.elwire.dance_floor, stats.elwire.entry);

  printf("%sfx: active(%s) next(%s) prev(%s)\n", indent, fxDesc(fx.active),
         fxDesc(fx.next), fxDesc(fx.prev));

  // dmx
  printf("\n");

  printf("%-*s%02.02fframes/s frame_µs(%6llu) expected_fps(%02.02f)\n",
         indent_size, "dmx:", dmx.fps, dmx.frame.us, dmx.frame.fps_expected);

  printf("%sframe: prepare_ms(%4.3f,%5.3f) update_ms(%4.1f,%5.1f) "
         "white_space_ms(%4.1f,%5.1f)\n",
         indent, dmx.frame.prepare_us.currentAsMillis(),
         dmx.frame.prepare_us.maxAsMillis(),
         dmx.frame.update_us.currentAsMillis(),
         dmx.frame.update_us.maxAsMillis(),
         dmx.frame.white_space_us.currentAsMillis(),
         dmx.frame.white_space_us.maxAsMillis());

  printf("%stx_ms(%5.2f,%5.2f,%5.2f) busy_wait(%llu) shorts(%llu)\n", indent,
         dmx.tx_ms.min(), dmx.tx_ms.current(), dmx.tx_ms.max(), dmx.busy_wait,
         dmx.frame.shorts);

  // i2s
  printf("\n");
  const float raw_kbps = i2s.rate.raw_bps.current() / 1024.0;
  printf("%-*s%.02f Kb/s %11.2f samples/s %11.2f fft/s\n", indent_size,
         "i2s:", raw_kbps, i2s.rate.samples_per_sec,
         i2s.rate.fft_per_sec.current());

  printf("%sdurations: fft_µs(%7llu,%7llu) rx_µs(%7llu,%7llu,%7llu)\n", indent,
         i2s.elapsed.fft_us.current(), i2s.elapsed.fft_us.max(),
         i2s.elapsed.rx_us.min(), i2s.elapsed.rx_us.current(),
         i2s.elapsed.rx_us.max());

  printf("%sraw left(%8d, %8d, %8d) right(%8d, %8d, %8d)\n", indent,
         i2s.raw_val_left.min(), i2s.raw_val_left.current(),
         i2s.raw_val_left.max(), i2s.raw_val_right.min(),
         i2s.raw_val_right.current(), i2s.raw_val_right.max());

  printf("%sfft: bin_width(%7.2fHz) dB(%8.2f,%8.2f,%8.2f) \n", indent,
         i2s.config.freq_bin_width, i2s.dB.min(), i2s.dB.current(),
         i2s.dB.max());

  printf("%sdB floor: generic(%6.1f) bass(%6.1f) complexity(%6.1f)\n", indent,
         i2s.config.dB_floor, i2s.config.bass_dB_floor,
         i2s.config.complexity_dB_floor);

  printf("%smpeak: freq(%8.2f) dB(%8.2f)\n", indent, i2s.mpeak.freq,
         i2s.mpeak.dB);

  printf("%scomplexity: instant(%5.2f) avg7sec(%5.2f)\n", indent,
         i2s.complexity.instant, i2s.complexity.avg7sec);

  printf("\n");

  return rc;
}

} // namespace pierre
