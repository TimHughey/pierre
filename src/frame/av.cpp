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

#include "av.hpp"
#include "base/io.hpp"
#include "base/shk.hpp"
#include "base/threads.hpp"
#include "base/typical.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "fft.hpp"
#include "frame.hpp"
#include "libav.hpp"
#include "peaks.hpp"
#include "types.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <iterator>
#include <latch>
#include <mutex>
#include <ranges>
#include <sodium.h>
#include <utility>
#include <vector>

namespace pierre {
namespace av { // encapsulation of libav*

constexpr auto module_id = csv("av::FRAME");

// namespace globals
AVCodec *codec = nullptr;
AVCodecContext *codec_ctx = nullptr;
int codec_open_rc = -1;
AVCodecParserContext *parser_ctx = nullptr;
AVPacket *pkt = nullptr;
SwrContext *swr = nullptr;

constexpr std::ptrdiff_t ADTS_HEADER_SIZE = 7;
constexpr int ADTS_PROFILE = 2;     // AAC LC
constexpr int ADTS_FREQ_IDX = 4;    // 44.1 KHz
constexpr int ADTS_CHANNEL_CFG = 2; // CPE

uint8_t *pcm_audio;
int dst_linesize = 0;

constexpr auto AV_FORMAT_IN = AV_SAMPLE_FMT_FLTP;
constexpr auto AV_FORMAT_OUT = AV_SAMPLE_FMT_S16;
constexpr auto AV_CH_LAYOUT = AV_CH_LAYOUT_STEREO;

void check_nullptr(void *ptr) {
  if (ptr == nullptr) {
    debug_dump();
    throw std::runtime_error("allocate failed");
  }
}

void debug_dump() {
  string msg;
  auto w = std::back_inserter(msg);

  constexpr auto f = FMT_STRING("{:>25}={}\n");
  fmt::format_to(w, f, "AVCodec", fmt::ptr(codec));
  fmt::format_to(w, f, "AVCodecContext", fmt::ptr(codec_ctx));
  fmt::format_to(w, f, "codec_open_rc", codec_open_rc);
  fmt::format_to(w, f, "AVCodecParserContext", fmt::ptr(parser_ctx));
  fmt::format_to(w, f, "AVPacket", fmt::ptr(pkt));
  fmt::format_to(w, f, "SwrContext", fmt::ptr(swr));

  __LOG0(LCOL01 "\n {}\n", module_id, csv("DEBUG DUMP"), msg);
}

void init() {
  static bool _initialized = false;

  if (_initialized == false) {
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    check_nullptr(codec);

    codec_ctx = avcodec_alloc_context3(codec);
    check_nullptr(codec_ctx);

    codec_open_rc = avcodec_open2(codec_ctx, codec, nullptr);
    if (codec_open_rc < 0) {
      debug_dump();
      throw std::runtime_error("avcodec_open2");
    }

    parser_ctx = av_parser_init(codec->id);
    check_nullptr(parser_ctx);

    pkt = av_packet_alloc();
    check_nullptr(pkt);

    swr = swr_alloc();
    check_nullptr(swr);

    av_opt_set_channel_layout(swr, "in_channel_layout", AV_CH_LAYOUT, 0);
    av_opt_set_channel_layout(swr, "out_channel_layout", AV_CH_LAYOUT, 0);
    av_opt_set_int(swr, "in_sample_rate", InputInfo::rate, 0);
    av_opt_set_int(swr, "out_sample_rate", InputInfo::rate, 0); // must match for timing
    av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_FORMAT_IN, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_FORMAT_OUT, 0);
    swr_init(swr);

    _initialized = true;
  }
}

bool keep(cipher_buff_t *m, size_t decipher_len, int used) {
  auto rc = true; // keep frame unless checks fail
  int32_t encoded_size = decipher_len + ADTS_HEADER_SIZE;

  if ((used < 0) || (used != encoded_size) || (pkt->size == 0)) {
    rc = false;

    constexpr auto decipher = csv("DECIPHER FAILED");
    constexpr auto malformed = csv("AAC MALFORMED");

    string msg;
    auto w = std::back_inserter(msg);
    fmt::format_to(w, "{} used={:<6} size={:<6}",         // fmt
                   pkt->size == 0 ? malformed : decipher, // log reason
                   used, encoded_size);

    // add used vs. packet size (if needed)
    if (int32_t diff = encoded_size - used; diff && (used > 0) && pkt->size) {
      fmt::format_to(w, " diff={:+6}", diff);
    }

    if (pkt->size == 0) { // AAC malformed, dump packet bytes
      for (auto idx = 0; idx < encoded_size; idx++) {
        if ((idx % 5) || (idx == 0)) { // new bytes row
          fmt::format_to(w, "\n{} ", __LOG_PREFIX);
        }

        fmt::format_to(w, "[{:<02}]0x{:<02x} ", idx, m->at(idx));
      }
    }

    __LOG0(LCOL01 " {}\n", module_id, "KEEP", msg);
  } // end of debug logging

  return rc;
}

// leave space for ADTS header

uint8_t *m_buffer(cipher_buff_ptr &m) { return m->data() + ADTS_HEADER_SIZE; }

void parse(frame_t frame) {
  auto &decoded = frame->decoded;
  decoded.clear(); // decoded will be empty on error

  auto decipher_len = frame->decipher_len;

  // NOTE: the size of encoded is the deciphered len AND the ADTS header
  const size_t encoded_size = decipher_len + ADTS_HEADER_SIZE;

  auto m = frame->m->data();

  // make ADTS header
  m[0] = 0xFF;
  m[1] = 0xF9;
  m[2] = ((ADTS_PROFILE - 1) << 6) + (ADTS_FREQ_IDX << 2) + (ADTS_CHANNEL_CFG >> 2);
  m[3] = ((ADTS_CHANNEL_CFG & 3) << 6) + (encoded_size >> 11);
  m[4] = (encoded_size & 0x7FF) >> 3;
  m[5] = ((encoded_size & 7) << 5) + 0x1F;
  m[6] = 0xFC;

  auto used = av_parser_parse2(parser_ctx,     // parser ctx
                               codec_ctx,      // codex ctx
                               &pkt->data,     // ptr to the pkt (parsed data)
                               &pkt->size,     // ptr size of the pkt (parsed data)
                               m,              // deciphered (unchanged by parsing)
                               encoded_size,   // deciphered size + ADTS header
                               AV_NOPTS_VALUE, // pts
                               AV_NOPTS_VALUE, // dts
                               0);             // pos

  if (keep(frame->m.get(), frame->decipher_len, used) == false) {
    return;
  }

  int ret = 0; // re-used for av* calls
  ret = avcodec_send_packet(codec_ctx, pkt);

  if (ret < 0) {
    __LOG0("{:<18} AV send packet failed decipher_len={} size={} flags={:#b} rc=0x{:x}\n", //
           module_id, decipher_len, pkt->size, pkt->flags, ret);
    return;
  }

  auto decoded_frame = av_frame_alloc();
  ret = avcodec_receive_frame(codec_ctx, decoded_frame);

  if (ret != 0) {
    __LOG0("{:<18} avcodec_receive_frame={}\n", module_id, ret);
    return;
  }

  frame->channels = codec_ctx->channels;

  av_samples_alloc(&pcm_audio,                // pointer to PCM samples
                   &dst_linesize,             // pointer to calculated linesize ?
                   frame->channels,           // num of channels
                   decoded_frame->nb_samples, // num of samples
                   AV_FORMAT_OUT,             // desired format (see const above)
                   0);                        // default alignment

  frame->samples_per_channel = swr_convert(           // perform the software resample
      swr,                                            // software resample ctx
      &pcm_audio,                                     // put processed samples here
      decoded_frame->nb_samples,                      // output this many samples
      (const uint8_t **)decoded_frame->extended_data, // process data at this ptr
      decoded_frame->nb_samples);                     // process this many samples

  auto dst_bufsize = av_samples_get_buffer_size // determine required buffer
      (&dst_linesize,                           // calc'ed linesize
       frame->channels,                         // num of channels
       frame->samples_per_channel,              // num of samples/channel (from swr_convert)
       AV_FORMAT_OUT,                           // desired format (see const above)
       1);                                      // default alignment

  __LOGX(LCOL01 " ret={} channels={} samples/channel={:<5} dst_buffsize={:<5}\n", //
         module_id, "INFO", ret, frame->channels, frame->samples_per_channel, dst_bufsize);

  if (dst_bufsize > 0) { // put PCM data into decoded
    auto to = std::back_inserter(decoded);
    ranges::copy_n(pcm_audio, dst_bufsize, to);

    __LOGX(LCOL01 " pcm data={} buf_size={:<5} payload_size={}\n", //
           module_id, "INFO", fmt::ptr(pcm_audio), dst_bufsize, payload.size());

    frame->state = frame::DECODED;
  }

  if (decoded_frame) {
    av_frame_free(&decoded_frame);
  }

  if (pcm_audio) {
    av_freep(&pcm_audio);
  }

  // we're done with m, reset (free) it
  frame->m.reset();
}

} // namespace av
} // namespace pierre
