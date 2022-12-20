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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#ifdef __cplusplus
}
#endif

#include "base/io.hpp"
#include "base/logger.hpp"
#include "dsp.hpp"
#include "fft.hpp"
#include "frame.hpp"

#include <atomic>
#include <memory>
namespace pierre {

class Av : public std::enable_shared_from_this<Av> {

private:
  static constexpr int ADTS_PROFILE{2};     // AAC LC
  static constexpr int ADTS_FREQ_IDX{4};    // 44.1 KHz
  static constexpr int ADTS_CHANNEL_CFG{2}; // CPE
  static constexpr std::ptrdiff_t ADTS_HEADER_SIZE{7};

private:
  Av(io_context &io_ctx) noexcept //
      : io_ctx(io_ctx),           //
        ready{false},             //
        dig_sig(Dsp::create())    //
  {}

  bool decode_failed(const frame_t &frame, AVPacket **pkt,
                     AVFrame **audio_frame = nullptr) noexcept {
    if (pkt) av_packet_free(pkt);
    if (audio_frame) av_frame_free(audio_frame);

    frame->state = frame::DECODE_FAILURE;
    frame->state.record_state();

    return false;
  }

  auto ptr() noexcept { return shared_from_this(); }

public:
  static auto create(io_context &io_ctx) noexcept {
    auto self = std::shared_ptr<Av>(new Av(io_ctx));
    auto s = self->ptr();

    asio::post(io_ctx, [s = std::move(s)]() {
      AVCodec *codec{nullptr};
      AVCodecContext *codec_ctx{nullptr};
      AVCodecParserContext *parser_ctx{nullptr};

      codec = avcodec_find_decoder(AV_CODEC_ID_AAC);

      if (codec) {
        codec_ctx = avcodec_alloc_context3(codec);

        if (codec_ctx) {
          if (auto rc = avcodec_open2(codec_ctx, codec, nullptr); rc < 0) {
            INFO(module_id, "CODEC_OPEN", "failed, rc={}\n", rc);
          } else [[likely]] {
            parser_ctx = av_parser_init(codec->id);

            if (parser_ctx) {
              s->codec = codec;
              s->codec_ctx = codec_ctx;
              s->parser_ctx = parser_ctx;

              s->ready.store(true);
            } else {
              INFO(module_id, "INIT", "failed to initialize AV functions\n");
            }
          } // end parser ctx
        }   // end open ctx
      }     // end codec ctx
    });

    return self;
  }

  /// @brief allocate space for the ADTS header and cipher
  /// @returns pointer to memory block for encrypted data
  static uint8_t *m_buffer(cipher_buff_t &m, ptrdiff_t bytes) noexcept {
    m.emplace(ADTS_HEADER_SIZE + bytes, 0x00);

    return m->data() + ADTS_HEADER_SIZE;
  }

  /// @brief resizes 'm' to accomodate bytes consumed by deciphering
  /// @param m buffer to resize
  /// @param consumed bytes deciphered
  static void m_buffer_resize(cipher_buff_t &m, unsigned long long consumed) noexcept {
    m->resize(consumed + ADTS_HEADER_SIZE);
  }

  /// @brief Parse (decode) deciphered frame to audio frame then perform FFT
  /// @param frame Frame to parse
  /// @return boolean indicating success or failure, Frame state will be set appropriately
  bool parse(frame_t frame) noexcept {

    auto rc = false;
    auto pkt = av_packet_alloc();

    if (!pkt) return decode_failed(frame, &pkt);

    auto m = frame->m->data();
    const size_t encoded_size = std::size(frame->m.value());

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
      log_discard(frame, used, pkt);
      return decode_failed(frame, &pkt);
    }

    if (auto rc = avcodec_send_packet(codec_ctx, pkt); rc < 0) {
      INFO(module_id, "SEND_PACKET", "FAILED encoded_size={} size={} flags={:#b} rc={}\n", //
           encoded_size, pkt->size, pkt->flags, rc);
      return decode_failed(frame, &pkt);
    }

    // allocate the av_frame that will receive the decoded audio data
    auto audio_frame = av_frame_alloc();
    if (!audio_frame) return decode_failed(frame, &pkt);

    if (auto rc = avcodec_receive_frame(codec_ctx, audio_frame); rc != 0) {
      INFO("FAILED rc={}\n", module_id, "RECV_FRAME", rc);
      return decode_failed(frame, &pkt, &audio_frame);
    }

    frame->channels = codec_ctx->channels;
    frame->samples_per_channel = audio_frame->nb_samples;

    log_diag_info(audio_frame);

    if (audio_frame->flags == 0) {
      frame->state = frame::DECODED;
      const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

      FFT left(data[0], frame->samples_per_channel, audio_frame->sample_rate);
      FFT right(data[1], frame->samples_per_channel, audio_frame->sample_rate);

      // this goes async
      dig_sig->process(frame, std::move(left), std::move(right));
      rc = true;
    }

    av_frame_free(&audio_frame);
    av_packet_free(&pkt);
    frame->m.reset();

    return rc;
  }

private:
  static void log_diag_info(AVFrame *audio_frame) noexcept {
    static bool reported = false;
    if (!reported) {
      const float *data[] = {(float *)audio_frame->data[0], (float *)audio_frame->data[1]};

      INFO(module_id, "INFO",
           "audio plane/linesize 1={}/{} 2={}/{} nb_samples={} format={} flags={}\n",
           fmt::ptr(data[0]), audio_frame->linesize[0], fmt::ptr(data[1]), audio_frame->linesize[1],
           audio_frame->nb_samples, audio_frame->format, audio_frame->flags);
      reported = true;
    }
  }

  static void log_discard(frame_t frame, int used, AVPacket *pkt) noexcept {
    int32_t enc_size = std::ssize(frame->m.value()) + ADTS_HEADER_SIZE;

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

private:
  // order dependent
  io_context &io_ctx;           // borrowed io_context
  std::atomic_bool ready;       // AV functionality setup and ready
  std::shared_ptr<Dsp> dig_sig; // digital signal processing

  // order independent
  AVCodec *codec{nullptr};
  AVCodecContext *codec_ctx{nullptr};
  AVCodecParserContext *parser_ctx{nullptr};

public:
  static constexpr csv module_id{"AV"};
};

} // namespace pierre