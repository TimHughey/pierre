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
#include "base/logger.hpp"
#include "fft.hpp"

namespace pierre {

Av::Av() noexcept : ready{false} {
  FFT::init();

  codec = avcodec_find_decoder(AV_CODEC_ID_AAC);

  INFO_INIT("sizeof={:>5} codec={}", sizeof(Av), fmt::ptr(codec));

  if (codec) {
    codec_ctx = avcodec_alloc_context3(codec);

    if (codec_ctx) {
      if (auto rc = avcodec_open2(codec_ctx, codec, nullptr); rc < 0) {
        INFO(module_id, "codec_open", "failed, rc={}", rc);
      } else [[likely]] {
        parser_ctx = av_parser_init(codec->id);

        if (parser_ctx) {
          ready.store(true);
        } else {
          INFO_INIT("failed to initialize AV functions");
        }
      } // end parser ctx
    }   // end open ctx
  }     // end codec ctx
}

Av::~Av() noexcept {
  av_parser_close(parser_ctx);
  avcodec_free_context(&codec_ctx);
}

bool Av::decode_failed(Frame &frame, AVPacket **pkt, AVFrame **audio_frame) noexcept {
  if (pkt) av_packet_free(pkt);
  if (audio_frame) av_frame_free(audio_frame);

  frame.state = frame::DECODE_FAILURE;
  frame.record_state();

  return false;
}

void Av::dsp(Frame &frame, FFT &&left, FFT &&right) noexcept {

  frame.state = frame::DSP_IN_PROGRESS;

  // the state hasn't changed, proceed with processing
  left.process();

  // check before starting the right channel (left required processing time)
  if (frame.state == frame::DSP_IN_PROGRESS) right.process();

  // check again since thr right channel also required processing time
  if (frame.state == frame::DSP_IN_PROGRESS) {
    left.find_peaks(frame.peaks, Peaks::CHANNEL::LEFT);

    if (frame.state == frame::DSP_IN_PROGRESS) {
      right.find_peaks(frame.peaks, Peaks::CHANNEL::RIGHT);
    }

    frame.silent(frame.peaks.silence() && frame.peaks.silence());

    frame.state = frame::DSP_COMPLETE;
  }
}

void Av::log_diag_info(AVFrame *audio_frame) noexcept {
  static bool reported = false;
  if (!reported) {
    const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

    INFO(module_id, "debug",
         "audio plane/linesize 1={}/{} 2={}/{} nb_samples={} format={} flags={}", fmt::ptr(data[0]),
         audio_frame->linesize[0], fmt::ptr(data[1]), audio_frame->linesize[1],
         audio_frame->nb_samples, audio_frame->format, audio_frame->flags);
    reported = true;
  }
}

void Av::log_discard(Frame &frame, int used) noexcept {
  int32_t enc_size = std::ssize(frame.m.value()) + ADTS_HEADER_SIZE;

  string msg;
  auto w = std::back_inserter(msg);

  if ((used < 0) || (used != enc_size)) {
    frame.state = frame::PARSE_FAILURE;

    fmt::format_to(w, "used={:<6} size={:<6} diff={:+6}", used, enc_size, enc_size - used);
  }

  INFO(module_id, "DISCARD", "{} {}", frame.state, msg);
}

bool Av::parse(Frame &frame) noexcept {

  auto rc = false;
  auto pkt = av_packet_alloc();

  if (!pkt) return decode_failed(frame, &pkt);

  auto m = frame.m->data();
  const size_t encoded_size = std::size(frame.m.value());

  // populate ADTS header
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

  if ((used <= 0) || std::cmp_not_equal(used, encoded_size) || (pkt->size == 0)) {
    log_discard(frame, used);
    return decode_failed(frame, &pkt);
  }

  if (auto rc = avcodec_send_packet(codec_ctx, pkt); rc < 0) {
    INFO(module_id, "SEND_PACKET", "FAILED encoded_size={} size={} flags={:#b} rc={}", //
         encoded_size, pkt->size, pkt->flags, rc);
    return decode_failed(frame, &pkt);
  }

  // allocate the av_frame that will receive the decoded audio data
  auto audio_frame = av_frame_alloc();
  if (!audio_frame) return decode_failed(frame, &pkt);

  if (auto rc = avcodec_receive_frame(codec_ctx, audio_frame); rc != 0) {
    INFO(module_id, "RECV_FRAME", " FAILED rc = {} ", rc);
    return decode_failed(frame, &pkt, &audio_frame);
  }

  frame.channels = codec_ctx->channels;
  frame.samples_per_channel = audio_frame->nb_samples;

  log_diag_info(audio_frame);

  if (audio_frame->flags == 0) {
    frame.state = frame::DECODED;
    const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

    FFT left(data[0], frame.samples_per_channel, audio_frame->sample_rate);
    FFT right(data[1], frame.samples_per_channel, audio_frame->sample_rate);

    dsp(frame, std::move(left), std::move(right));
    rc = true;
  }

  av_frame_free(&audio_frame);
  av_packet_free(&pkt);
  frame.m.reset();

  return rc;
}

} // namespace pierre