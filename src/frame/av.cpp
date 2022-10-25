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
#include "base/types.hpp"
#include "base/uint8v.hpp"
#include "dsp.hpp"
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
SwrContext *swr = nullptr;

constexpr std::ptrdiff_t ADTS_HEADER_SIZE = 7;
constexpr int ADTS_PROFILE = 2;     // AAC LC
constexpr int ADTS_FREQ_IDX = 4;    // 44.1 KHz
constexpr int ADTS_CHANNEL_CFG = 2; // CPE

constexpr auto AV_FORMAT_IN = AV_SAMPLE_FMT_FLTP;
constexpr auto AV_FORMAT_OUT = AV_SAMPLE_FMT_S16;
constexpr auto AV_CH_LAYOUT = AV_CH_LAYOUT_STEREO;

// forward decls
void debug_dump();
void log_discard(cipher_buff_t *m, size_t decipher_len, int used, AVPacket *pkt);

template <typename T> static constexpr T *check_nullptr(T *ptr) {
  if (ptr == nullptr) {
    debug_dump();
    throw std::runtime_error("allocate failed");
  }

  return ptr;
}

void debug_dump() {
  string msg;
  auto w = std::back_inserter(msg);

  constexpr auto f = FMT_STRING("{:>25}={}\n");
  fmt::format_to(w, f, "AVCodec", fmt::ptr(codec));
  fmt::format_to(w, f, "AVCodecContext", fmt::ptr(codec_ctx));
  fmt::format_to(w, f, "codec_open_rc", codec_open_rc);
  fmt::format_to(w, f, "AVCodecParserContext", fmt::ptr(parser_ctx));
  fmt::format_to(w, f, "SwrContext", fmt::ptr(swr));

  INFO(module_id, "DEBUG DUMP", "\n {}\n", msg);
}

void init() {
  static bool _initialized = false;

  if (_initialized == false) {
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    check_nullptr(codec);

    codec_ctx = avcodec_alloc_context3(codec);
    check_nullptr(codec_ctx);

    // codec_ctx->thread_count = 3;
    // codec_ctx->thread_type = FF_THREAD_FRAME;

    codec_open_rc = avcodec_open2(codec_ctx, codec, nullptr);
    if (codec_open_rc < 0) {
      debug_dump();
      throw std::runtime_error("avcodec_open2");
    }

    parser_ctx = av_parser_init(codec->id);
    check_nullptr(parser_ctx);

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

void log_discard(frame_t frame, int used, AVPacket *pkt) {
  int32_t enc_size = frame->decipher_len + ADTS_HEADER_SIZE;

  string msg;
  auto w = std::back_inserter(msg);

  if ((used < 0) || (used != enc_size)) {
    frame->state = frame::PARSE_FAILURE;

    fmt::format_to(w, "used={:<6} size={:<6} diff={:+6}", used, enc_size, enc_size - used);

    if (pkt->size == 0) {
      auto m = frame->m->data();
      for (auto idx = 0; idx < enc_size; idx++) {
        if ((idx % 5) || (idx == 0)) { // new bytes row
          fmt::format_to(w, "\n");
        }

        fmt::format_to(w, "[{:<02}]0x{:<02x} ", idx, m[idx]);
      }
    }
  }

  const auto chunk = INFO_FORMAT_CHUNK(msg.data(), msg.size());
  INFO(module_id, "DISCARD", "{}\n{}", frame->state, msg);
}

// leave space for ADTS header

uint8_t *m_buffer(cipher_buff_ptr &m) { return m->data() + ADTS_HEADER_SIZE; }

void parse(frame_t frame) {
  auto pkt = check_nullptr(av_packet_alloc());

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

  auto used = av_parser_parse2(parser_ctx,      // parser ctx
                               codec_ctx,       // codex ctx
                               &pkt->data,      // ptr to the pkt (parsed data)
                               &pkt->size,      // ptr size of the pkt (parsed data)
                               m,               // deciphered (unchanged by parsing)
                               encoded_size,    // deciphered size + ADTS header
                               AV_NOPTS_VALUE,  // pts
                               AV_NOPTS_VALUE,  // dts
                               AV_NOPTS_VALUE); // pos

  if ((used < 0) || std::cmp_not_equal(used, encoded_size) || (pkt->size == 0)) {
    log_discard(frame, used, pkt);
    av_packet_free(&pkt);
    return;
  }

  if (auto rc = avcodec_send_packet(codec_ctx, pkt); rc < 0) {
    INFO(module_id, "SEND_PACKET", "FAILED decipher_len={} size={} flags={:#b} rc={}\n", //
         decipher_len, pkt->size, pkt->flags, rc);
    frame->state = frame::DECODE_FAILURE;
    av_packet_free(&pkt);
    return;
  }

  auto audio_frame = check_nullptr(av_frame_alloc());
  if (auto rc = avcodec_receive_frame(codec_ctx, audio_frame); rc != 0) {
    INFO("FAILED rc={}\n", module_id, "RECV_FRAME", rc);
    av_packet_free(&pkt);
    return;
  }

  frame->channels = codec_ctx->channels;
  frame->samples_per_channel = audio_frame->nb_samples;

  // int err = av_samples_alloc(&pcm_audio,              // pointer to PCM samples
  //                            &dst_linesize,           // pointer to calculated linesize ?
  //                            frame->channels,         // num of channels
  //                            audio_frame->nb_samples, // num of samples
  //                            AV_FORMAT_OUT,           // desired format (see const above)
  //                            0);                      // default alignment
  //
  // if (err >= 0) {
  //   frame->samples_per_channel = swr_convert(         // perform the software resample
  //       swr,                                          // software resample ctx
  //       &pcm_audio,                                   // put processed samples here
  //       audio_frame->nb_samples,                      // output this many samples
  //       (const uint8_t **)audio_frame->extended_data, // process data at this ptr
  //       audio_frame->nb_samples);                     // process this many samples
  //
  //   auto dst_bufsize = av_samples_get_buffer_size // determine required buffer
  //       (&dst_linesize,                           // calc'ed linesize
  //        frame->channels,                         // num of channels
  //        frame->samples_per_channel,              // num of samples/channel (from swr_convert)
  //        AV_FORMAT_OUT,                           // desired format (see const above)
  //        0);                                      // default alignment

  static bool reported = false;
  if (!reported) {
    // INFO(  "err={} ch={} linesize={:<5} samples/ch={:<5} dst_buffsize={:<5}\n", //
    //        module_id, "INFO", err, frame->channels, dst_linesize, frame->samples_per_channel,
    //        dst_bufsize);

    const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

    INFO(module_id, "INFO",
         "audio plane/linesize 1={}/{} 2={}/{} nb_samples={} format={} flags={}\n",
         fmt::ptr(data[0]), audio_frame->linesize[0], fmt::ptr(data[1]), audio_frame->linesize[1],
         audio_frame->nb_samples, audio_frame->format, audio_frame->flags);
    reported = true;
  }

  if (audio_frame->flags == 0) {
    frame->state = frame::DECODED;
    const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

    FFT left(data[0], frame->samples_per_channel, audio_frame->sample_rate);
    FFT right(data[1], frame->samples_per_channel, audio_frame->sample_rate);

    // this goes async
    dsp::process(frame, std::move(left), std::move(right));
  }

  av_frame_free(&audio_frame);
  frame->m.reset();
}

} // namespace av
} // namespace pierre
