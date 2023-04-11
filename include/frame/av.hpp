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

#include "dsp.hpp"
#include "fft.hpp"
#include "frame.hpp"
#include "lcs/logger.hpp"
#include "libav.hpp"

#include <atomic>
#include <memory>
#include <optional>

namespace pierre {

class Av {

public:
  Av() noexcept;
  ~Av() noexcept;

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
  bool parse(frame_t frame) noexcept;

private:
  bool decode_failed(const frame_t &frame, AVPacket **pkt,
                     AVFrame **audio_frame = nullptr) noexcept;

  void log_diag_info(AVFrame *audio_frame) noexcept;

  static void log_discard(frame_t frame, int used) noexcept;

private:
  static constexpr int ADTS_PROFILE{2};     // AAC LC
  static constexpr int ADTS_FREQ_IDX{4};    // 44.1 KHz
  static constexpr int ADTS_CHANNEL_CFG{2}; // CPE
  static constexpr std::ptrdiff_t ADTS_HEADER_SIZE{7};

private:
  // order dependent
  std::atomic_bool ready; // AV functionality setup and ready
  Dsp dsp;                // digital signal processing

  // order independent
  AVCodec *codec{nullptr};
  AVCodecContext *codec_ctx{nullptr};
  AVCodecParserContext *parser_ctx{nullptr};

public:
  static constexpr csv module_id{"frame.av"};
};

} // namespace pierre