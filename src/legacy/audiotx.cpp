/*
    pierre.cpp - Audio Transmission
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

#include <fstream>
#include <iostream>
#include <ostream>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/timer/timer.hpp>

#include "audio/audiotx.hpp"
#include "external/ArduinoJson.h"
#include "misc/maverage.hpp"

using namespace pierre;
using ofstream = std::ofstream;
using auto_cpu_timer = boost::timer::auto_cpu_timer;

using std::cerr, std::endl;

AudioTx::~AudioTx() {
  if (_pcm) {
    snd_pcm_close(_pcm);
    _pcm = nullptr;
  }

  if (_params) {
    snd_pcm_hw_params_free(_params);
    _params = nullptr;
  }

  if (_swparams) {
    snd_pcm_sw_params_free(_swparams);
    _swparams = nullptr;
  }
}

void AudioTx::audioInThread() {
  int snd_rc = -1;

  while (true) {
    snd_rc = snd_pcm_wait(_pcm, 100);

    if (snd_rc < 0) {
      if (snd_rc == 0) {
        cerr << "PCM capture timeout\n";
      } else {
        recoverStream(snd_rc);
      }
      continue;
    }

    auto frames_ready = snd_pcm_avail_update(_pcm);

    if (frames_ready <= 0) {
      recoverStream(frames_ready);
      continue;
    }

    // frames are ready for capture
    auto bytes_ready = framesToBytes(frames_ready);
    ptrByteBuffer buff(new ByteBuffer_t);

    // to use the vector as a buffer
    buff.get()->resize(bytes_ready);
    buff.get()->assign(bytes_ready, 0x00); // zero the buffer

    auto raw = buff.get()->data();
    auto frames = snd_pcm_mmap_readi(_pcm, raw, frames_ready);

    // audio data was captured
    if (frames > 0) {
      auto bytes_actual = snd_pcm_frames_to_bytes(_pcm, frames);
      if (bytes_actual != bytes_ready) {
        cerr << "pcm reported " << bytes_ready << " but only read " << bytes_actual;
        buff.get()->resize(bytes_actual);
      }

      _net_out_q.push(buff);
      _fft_q.push(buff);
    } else {
      // frames is the error code
      recoverStream(frames);
    }
  }
}

void AudioTx::dmxThread() {
  using boost::array;
  using boost::asio::buffer;
  using boost::asio::io_context;
  using boost::asio::error::basic_errors;
  using boost::asio::ip::tcp;
  using boost::system::error_code;
  using boost::system::system_error;
  using std::chrono::milliseconds;

  io_context io_ctx;
  tcp::resolver resolver(io_ctx);

  tcp::resolver::results_type endpoints = resolver.resolve(_dest_host, _dmx_port);

  for (auto entry : endpoints) {
    cerr << entry.host_name() << endl;
  }

  error_code conn_ec;
  uint retries = 0;
  bool aborting = false;
  do {
    tcp::socket socket(io_ctx);
    boost::asio::connect(socket, endpoints, conn_ec);

    // if (conn_ec == basic_errors::connection_refused) {
    if (conn_ec) {
      if (retries > 5) {
        aborting = true;
      } else {
        auto le = socket.local_endpoint();

        cerr << le.address() << " " << conn_ec.message() << endl;

        std::this_thread::sleep_for(milliseconds(1000));
        retries++;
      }

      continue;
    }

    for (; socket.is_open();) {
      error_code read_ec;
      array<char, 1024> buff;

      socket.read_some(buffer(buff), read_ec);

      if (read_ec) {
        cerr << read_ec.message() << endl;
        break;
      }

      StaticJsonDocument<2048> doc;
      DeserializationError err = deserializeMsgPack(doc, buff);

      if (err) {
        cerr << err << endl;
        continue;
      }

      const JsonObject &frame_obj = doc["frame"];

      if (frame_obj["prepare"] == true) {
        array<uint8_t, 127> frame = {{0}};

        auto frame_buffer = buffer(frame);
        uint8_t *x = static_cast<uint8_t *>(frame_buffer.data());

        *x++ = 0xF0;
        *x++ = 0xFF;
        *x++ = 0xFF;
        *x++ = 0x00;
        *x++ = 0xFF;

        socket.write_some(buffer(frame));
      }

      // if (!err) {
      //   cerr << doc << endl;
      // }
    }

    // switch (ec.value()) {
    // case boost::asio::error::connection_refused:
    //   cerr << ec.message() << endl;
    //   break;
    // }

  } while (!aborting);

  cerr << "thread exiting, " << conn_ec.message() << endl;
}

void AudioTx::FFTThread() {
  ofstream fft_log(_fft_log, ofstream::out | ofstream::trunc);

  MovingAverage<float> mavg(1);
  mavg.addValue(1.0);
  mavg.addValue(2.0);

  // iterator that points to position to put next sample
  auto left_pos = _fft_left.real().begin();
  auto right_pos = _fft_right.real().begin();

  auto left_end = _fft_left.real().cend();

  while (true) {
    auto data = _fft_q.pop(); // wait for and pop the next buffer

    // copy the data buffer to the net buffer until the net buffer is full
    auto byte = data.get()->cbegin();
    auto end = data.get()->cend();

    while (byte != end) {
      if (left_pos == left_end) {
        //        auto_cpu_timer t(fft_log, 5, "%t sec CPU, %w sec real\n");
        // got enough data for FFT

        _fft_left.process();
        _fft_right.process();

        // reset pos to reuse the buffer
        left_pos = _fft_left.real().begin();
        right_pos = _fft_right.real().begin();

        const Peaks_t &peaks = _fft_left.peaks();
        const PeakInfo mpeak = _fft_left.majorPeak(peaks);

        fft_log << "left peak[" << mpeak.freq << "] dB[" << mpeak.mag << "] ";
      }

      fft_log.flush();

      *left_pos++ = static_cast<float>(*byte++ + (*byte++ << 8));
      *right_pos++ = static_cast<float>(*byte++ + (*byte++ << 8));
    }
  }
}

bool AudioTx::init() {
  auto rc = true;
  const auto stream = SND_PCM_STREAM_CAPTURE;
  int open_mode = 0;

  auto err = snd_pcm_open(&_pcm, pcm_name, stream, open_mode);
  if (err < 0) {
    cerr << "audio open error: " << snd_strerror(err);
    rc = false;
  }

  err = snd_output_stdio_attach(&_pcm_log, stderr, 0);

  setParams();

  snd_pcm_start(_pcm);

  if (isRunning() == false) {
    cerr << "[FATAL] PCM is not in running state";
    rc = false;
  }

  return rc;
}

void AudioTx::netOutThread() {
  // allocate and zero the initial network buffer
  ptrByteBuffer net_buff(new ByteBuffer_t);
  net_buff.get()->resize(_net_packet_size);
  net_buff.get()->assign(_net_packet_size, 0x00);

  // iterator that always points to position to where the next data copy
  // will begin
  auto net_buff_pos = net_buff.get()->begin();

  while (true) {
    auto data = _net_out_q.pop(); // wait for and pop the next buffer

    // copy the data buffer to the net buffer until the net buffer is full
    for (const uint8_t byte : *data.get()) {
      if (net_buff_pos == net_buff.get()->cend()) {
        // net buff is full, send it...
        _net_raw.send(*net_buff);
        // const auto raw = net_buff.get()->data();
        // const auto len = net_buff.get()->size();
        // auto sent = sendto(_send_socket, raw, len, 0, _dest_ipv4,
        // _dest_size);
        //
        // if ((uint)sent != len) {
        //   cerr << "[WARN] sent " << sent << " of " << len << " bytes";
        // }

        // fprintf(stderr, "sent %ld bytes of %lu\n", tx_bytes, raw_bytes);

        // reset net_buff_pos to reuse the buffer
        net_buff_pos = net_buff.get()->begin();

        // keep copying the data
      }
      // the net_buff isn't full yet, copy the data to it
      *net_buff_pos = byte;

      net_buff_pos++;
    }
  }
}

bool AudioTx::run() {
  audio_in = std::jthread([this]() { this->audioInThread(); });
  dmx = std::jthread([this]() { this->dmxThread(); });
  fft_calc = std::jthread([this]() { this->FFTThread(); });
  net_out = std::jthread([this]() { this->netOutThread(); });

  audio_in.join();
  dmx.join();
  net_out.join();
  fft_calc.join();

  return true;
}

bool AudioTx::setParams(void) {
  bool rc = true;
  int err;

  snd_pcm_access_mask_t *mask = nullptr;

  err = snd_pcm_hw_params_any(_pcm, _params);
  if (err < 0) {
    cerr << "PCM: no configurations available";
    return false;
  }

  mask = (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
  snd_pcm_access_mask_none(mask);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
  snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
  err = snd_pcm_hw_params_set_access_mask(_pcm, _params, mask);

  if (err < 0) {
    cerr << "access type not available";
    return false;
  }

  err = snd_pcm_hw_params_set_format(_pcm, _params, format());
  if (err < 0) {
    cerr << "sample format not available";
    return false;
  }

  err = snd_pcm_hw_params_set_channels(_pcm, _params, channels());
  if (err < 0) {
    cerr << "channel count not available";
    return false;
  }

  err = snd_pcm_hw_params_set_rate_near(_pcm, _params, &(_config.rate), 0);

  snd_pcm_uframes_t psize_min = 0;
  if (snd_pcm_hw_params_get_period_size_min(_params, &psize_min, 0) >= 0) {
    snd_pcm_hw_params_set_period_size(_pcm, _params, psize_min, 0);
  }

  // reportBufferMin();

  const snd_pcm_uframes_t buff_size = 4096;
  snd_pcm_hw_params_set_buffer_size(_pcm, _params, buff_size);

  _monotonic = snd_pcm_hw_params_is_monotonic(_params);
  _can_pause = snd_pcm_hw_params_can_pause(_params);

  err = snd_pcm_hw_params(_pcm, _params);
  if (err < 0) {
    cerr << "unable to install hw params:";
    snd_pcm_hw_params_dump(_params, _pcm_log);
    return false;
  }

  snd_pcm_hw_params_get_periods(_params, &_periods, 0);

  err = snd_pcm_sw_params_current(_pcm, _swparams);
  if (err < 0) {
    cerr << "unable to get current sw params.";
    return false;
  }

  err = snd_pcm_sw_params_set_avail_min(_pcm, _swparams, 128);

  constexpr snd_pcm_uframes_t sthres_max = 512;
  const snd_pcm_uframes_t sthres = std::min(buff_size / 2, sthres_max);
  err = snd_pcm_sw_params_set_start_threshold(_pcm, _swparams, sthres);
  assert(err >= 0);

  if (snd_pcm_sw_params(_pcm, _swparams) < 0) {
    cerr << "unable to install sw params:";
    snd_pcm_sw_params_dump(_swparams, _pcm_log);
    return false;
  }

  // snd_pcm_dump(_pcm, _pcm_log);

  chunk_bytes = framesToBytes(_chunk_size);

  /* show mmap buffer arrangment */
  // const snd_pcm_channel_area_t *areas;
  // snd_pcm_uframes_t offset, size = _chunk_size;
  // int i;
  // err = snd_pcm_mmap_begin(_pcm, &areas, &offset, &size);
  // if (err < 0) {
  //   fprintf(stderr, "snd_pcm_mmap_begin problem: %s", snd_strerror(err));
  //   return false;
  // }
  // for (i = 0; i < channels(); i++)
  //   fprintf(stderr, "mmap_area[%i] = %p,%u,%u (%u)\n", i, areas[i].addr,
  //           areas[i].first, areas[i].step,
  //           snd_pcm_format_physical_width(format()));
  // /* not required, but for sure */
  // snd_pcm_mmap_commit(_pcm, offset, 0);

  testPosition(buff_size);

  return rc;
}

void AudioTx::testPosition(snd_pcm_uframes_t buffer_frames) {
  constexpr int coef = 8;
  static long counter = 0;
  static time_t tmr = -1;
  time_t now;
  static float availsum, delaysum, samples;
  static snd_pcm_sframes_t maxavail, maxdelay;
  static snd_pcm_sframes_t minavail, mindelay;
  static snd_pcm_sframes_t badavail = 0, baddelay = 0;
  snd_pcm_sframes_t outofrange;
  snd_pcm_sframes_t avail, delay;
  int err;

  err = snd_pcm_avail_delay(_pcm, &avail, &delay);

  if (avail || delay || err) {
    cerr << "err[" << err << "] avail [" << avail << "] avail_delay[" << delay << "]";
  }

  if (err < 0) {
    return;
  }

  outofrange = (coef * (snd_pcm_sframes_t)buffer_frames) / 2;
  if (avail > outofrange || avail < -outofrange || delay > outofrange || delay < -outofrange) {
    badavail = avail;
    baddelay = delay;
    availsum = delaysum = samples = 0;
    maxavail = maxdelay = 0;
    minavail = mindelay = buffer_frames * 16;
    cerr << "Suspicious buffer position (" << ++counter << "% total): avail = "
         << " delay = " << delay << ", buffer = " << buffer_frames;
  }

  time(&now);
  if (tmr == (time_t)-1) {
    tmr = now;
    availsum = delaysum = samples = 0;
    maxavail = maxdelay = 0;
    minavail = mindelay = buffer_frames * 16;
  }
  if (avail > maxavail)
    maxavail = avail;

  if (delay > maxdelay)
    maxdelay = delay;

  if (avail < minavail)
    minavail = avail;

  if (delay < mindelay)
    mindelay = delay;

  availsum += avail;
  delaysum += delay;
  samples++;
  if (avail != 0 && now != tmr) {
    cerr << "BUFPOS: avg=" << long(availsum / samples) << "min=" << minavail << "max=" << maxavail
         << "(" << buffer_frames << ") (" << counter << ":" << badavail << "/" << baddelay;
    tmr = now;
  }
}

// bool AudioTx::process() {
//   auto rc = true;
//
//   snd_pcm_start(_pcm);
//
//   if (isRunning() == false) {
//     fprintf(stderr, "[FATAL] PCM is not running\n");
//     return false;
//   }
//
//   size_t raw_count = 0;
//   while (true) {
//     snd_pcm_sframes_t avail, delay;
//
//     auto wait_rc = snd_pcm_wait(_pcm, 100);
//
//     if (wait_rc == 1) {
//       snd_pcm_avail_delay(_pcm, &avail, &delay);
//
//       auto sframes_read = snd_pcm_mmap_readi(_pcm, audiobuf + raw_count,
//       128);
//
//       if (sframes_read > 0) {
//         raw_count += framesToBytes(sframes_read);
//
//         if (raw_count >= 1024) {
//           auto tx_bytes = sendto(_send_socket, audiobuf, raw_count, 0,
//                                  _dest_ipv4, sizeof(_dest));
//
//           if (tx_bytes != raw_count) {
//             fprintf(stderr, "only sent %ld bytes\n", tx_bytes);
//           }
//
//           raw_count = 0;
//         }
//       }
//
//       // fprintf(stderr, "bytes_avail[%lu] bytes_read[%ld]
//       delay_bytes[%lu]\n",
//       //         framesToBytes(avail), framesToBytes(sframes_read),
//       //         framesToBytes(delay));
//     } else {
//       auto recover_rc = snd_pcm_recover(_pcm, wait_rc, 0);
//       if (recover_rc < 0) {
//         char err[256] = {0};
//         perror(err);
//         fprintf(stderr, "recover_rc: %s\n", err);
//         abort();
//       }
//     }
//   }
//
//   return rc;
// }
