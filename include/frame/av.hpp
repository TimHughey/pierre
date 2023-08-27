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

#include "base/conf/token.hpp"
#include "base/uint8v.hpp"
#include "frame/frame.hpp"
#include "libav.hpp"

#include <atomic>
#include <memory>
#include <optional>

namespace pierre {

// forward decls
// class FFT2;

class Av {

  static constexpr int ADTS_CHANNEL_CFG{2}; // CPE
  static constexpr int ADTS_FREQ_IDX{4};    // 44.1 KHz
  static constexpr int ADTS_PROFILE{2};     // AAC LC
  static constexpr std::ptrdiff_t ADTS_HEADER_SIZE{7};
  static constexpr std::ptrdiff_t CIPHER_BUFF_SIZE{0x2000};

public:
  Av() noexcept;
  ~Av() noexcept;

  /// @brief Decode deciphered frame to audio frame then perform FFT
  /// @param frame Frame to parse
  /// @return reference to frame state
  frame_state_v decode(Frame &frame, uint8v encoded) noexcept;

  static uint8v make_m_buffer() noexcept { return uint8v(CIPHER_BUFF_SIZE); }

  /// @brief allocate space for the ADTS header and cipher
  /// @returns pointer to memory block for encrypted data
  static auto *m_buffer(uint8v &m) noexcept {
    std::fill_n(m.begin(), ADTS_HEADER_SIZE, 0x00);

    return m.raw_buffer<uint8_t>(ADTS_HEADER_SIZE);
  }

  /// @brief resizes 'm' to accomodate bytes consumed by deciphering
  /// @param m buffer to resize
  /// @param consumed bytes deciphered
  static void m_buffer_resize(uint8v &m, ull_t consumed) noexcept {
    m.resize(ADTS_HEADER_SIZE + consumed);
  }

private:
  frame_state_v decode_failed(Frame &frame, AVPacket **pkt,
                              AVFrame **audio_frame = nullptr) noexcept;

  // frame_state_v dsp(Frame &frame, FFT2 &&left, FFT2 &&right) noexcept;

  void log_diag_info(AVFrame *audio_frame) noexcept;

  static void log_discard(Frame &frame, const uint8v &m, int used) noexcept;

private:
  // order dependent
  conf::token tokc;
  std::unique_ptr<frame::av_conf> conf;
  std::atomic_flag ready; // AV functionality setup and ready

  // order independent
  AVCodec *codec{nullptr};
  AVCodecContext *codec_ctx{nullptr};
  AVCodecParserContext *parser_ctx{nullptr};

public:
  MOD_ID("frame.av");
};

} // namespace pierre