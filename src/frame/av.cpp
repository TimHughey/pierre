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
        INFO("codec_open", "failed, rc={}", rc);
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

frame_state_v Av::decode(Frame &frame, uint8v m) noexcept {

  auto pkt = av_packet_alloc();

  if (!pkt) return decode_failed(frame, &pkt);

  // populate ADTS header
  uint8_t *adts = m.raw_buffer<uint8_t>();

  adts[0] = 0xFF;
  adts[1] = 0xF9;
  adts[2] = ((ADTS_PROFILE - 1) << 6) + (ADTS_FREQ_IDX << 2) + (ADTS_CHANNEL_CFG >> 2);
  adts[3] = ((ADTS_CHANNEL_CFG & 3) << 6) + (m.size1<int>() >> 11);
  adts[4] = (m.size1<int>() & 0x7FF) >> 3;
  adts[5] = ((m.size1<int>() & 7) << 5) + 0x1F;
  adts[6] = 0xFC;

  auto used = av_parser_parse2(parser_ctx,       // parser ctx
                               codec_ctx,        // codex ctx
                               &pkt->data,       // ptr to the pkt (parsed data)
                               &pkt->size,       // ptr size of the pkt (parsed data)
                               m.raw<uint8_t>(), // deciphered (unchanged by parsing)
                               m.size1<int>(),   // deciphered size + ADTS header
                               AV_NOPTS_VALUE,   // pts
                               AV_NOPTS_VALUE,   // dts
                               AV_NOPTS_VALUE);  // pos

  if ((used <= 0) || std::cmp_not_equal(used, m.size1<int>()) || (pkt->size == 0)) {
    log_discard(frame, m, used);
    return decode_failed(frame, &pkt);
  }

  if (auto rc = avcodec_send_packet(codec_ctx, pkt); rc < 0) {
    INFO("SEND_PACKET", "FAILED encoded_size={} size={} flags={:#b} rc={}", //
         m.size1<int>(), pkt->size, pkt->flags, rc);
    return decode_failed(frame, &pkt);
  }

  // allocate the av_frame that will receive the decoded audio data
  auto audio_frame = av_frame_alloc();
  if (!audio_frame) return decode_failed(frame, &pkt);

  if (auto rc = avcodec_receive_frame(codec_ctx, audio_frame); rc != 0) {
    INFO("RECV_FRAME", " FAILED rc = {} ", rc);
    return decode_failed(frame, &pkt, &audio_frame);
  }

  frame.save_samples_info({codec_ctx->channels, audio_frame->nb_samples});

  log_diag_info(audio_frame);

  if (audio_frame->flags == 0) {
    const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

    FFT left(data[0], audio_frame->nb_samples, audio_frame->sample_rate);
    FFT right(data[1], audio_frame->nb_samples, audio_frame->sample_rate);

    dsp(frame, std::move(left), std::move(right));
  }

  av_frame_free(&audio_frame);
  av_packet_free(&pkt);

  return (frame_state_v)frame;
}

frame_state_v Av::decode_failed(Frame &frame, AVPacket **pkt, AVFrame **audio_frame) noexcept {
  if (pkt) av_packet_free(pkt);
  if (audio_frame) av_frame_free(audio_frame);

  frame = frame::DECODE_FAIL;
  return (frame_state_v)frame.record_state();
}

frame_state_v Av::dsp(Frame &frame, FFT &&left, FFT &&right) noexcept {

  // the state hasn't changed, proceed with processing
  left.process();
  left.find_peaks(frame.peaks, Peaks::CHANNEL::LEFT);

  right.process();
  right.find_peaks(frame.peaks, Peaks::CHANNEL::RIGHT);

  frame.silent(frame.peaks.silence() && frame.peaks.silence());

  frame = frame::DSP;

  return (frame_state_v)frame;
}

void Av::log_diag_info(AVFrame *audio_frame) noexcept {
  static bool reported = false;
  if (!reported) {
    const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

    INFO("debug", "audio plane/linesize 1={}/{} 2={}/{} nb_samples={} format={} flags={}",
         fmt::ptr(data[0]), audio_frame->linesize[0], fmt::ptr(data[1]), audio_frame->linesize[1],
         audio_frame->nb_samples, audio_frame->format, audio_frame->flags);
    reported = true;
  }
}

void Av::log_discard(Frame &frame, const uint8v &m, int used) noexcept {
  auto buff_size = std::ssize(m) + ADTS_HEADER_SIZE;

  string msg;
  auto w = std::back_inserter(msg);

  if ((used < 0) || (used != buff_size)) {
    frame = frame::PARSE_FAIL;

    fmt::format_to(w, "used={:<6} size={:<6} diff={:+6}", used, buff_size, buff_size - used);
  }

  INFO("DISCARD", "{} {}", frame, msg);
}

} // namespace pierre