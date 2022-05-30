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

#include "packet/rtp.hpp"
#include "core/input_info.hpp"
#include "packet/basic.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fmt/format.h>
#include <iterator>
#include <mutex>
#include <ranges>
#include <sodium.h>
#include <utility>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
}
#endif

namespace pierre {
namespace packet {

namespace ranges = std::ranges;

namespace rtp {
uint32_t to_uint32(Basic::iterator begin, long int count) {
  uint32_t val = 0;
  size_t shift = (count - 1) * 8;

  for (auto it = std::counted_iterator{begin, count}; it != std::default_sentinel;
       ++it, shift -= 8) {
    val += *it << shift;
  }

  return val;
}

packet::Basic shk; // class level shared key
} // namespace rtp

namespace av { // encapsulation of libav*

// forward decls
void check_nullptr(void *ptr);
void debugDump();
void init(); // throws on alloc failures
uint8_t *mBuffer();
void parse(shRTP rtp);
bool keep(const CipherBuff &m, size_t decipher_len, int used, AVPacket *pkt);

// namespace globals
AVCodec *codec = nullptr;
AVCodecContext *codec_ctx = nullptr;
int codec_open_rc = -1;
AVCodecParserContext *parser_ctx = nullptr;
AVPacket *pkt = nullptr;
SwrContext *swr = nullptr;

constexpr size_t ADTS_HEADER_SIZE = 7;
constexpr int ADTS_PROFILE = 2;     // AAC LC
constexpr int ADTS_FREQ_IDX = 4;    // 44.1 KHz
constexpr int ADTS_CHANNEL_CFG = 2; // CPE

uint8_t *pcm_audio;
int dst_linesize = 0;

constexpr auto AV_FORMAT = AV_SAMPLE_FMT_S16;

void check_nullptr(void *ptr) {
  if (ptr == nullptr) {
    debugDump();
    throw std::runtime_error("allocate failed");
  }
}

void debugDump() {
  constexpr auto f1 = FMT_STRING("{} {}\n");
  fmt::print(f1, runTicks(), fnName());

  constexpr auto f2 = FMT_STRING("{:>25}={:}\n");
  fmt::print(f2, "AVCodec", fmt::ptr(codec));
  fmt::print(f2, "AVCodecContext", fmt::ptr(codec_ctx));
  fmt::print(f2, "codec_open_rc", codec_open_rc);
  fmt::print(f2, "AVCodecParserContext", fmt::ptr(parser_ctx));
  fmt::print(f2, "AVPacket", fmt::ptr(pkt));
  fmt::print(f2, "SwrContext", fmt::ptr(swr));
}

void init() {
  static std::once_flag __flag;

  std::call_once(__flag, [&]() {
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    check_nullptr(codec);

    codec_ctx = avcodec_alloc_context3(codec);
    check_nullptr(codec_ctx);

    codec_open_rc = avcodec_open2(codec_ctx, codec, nullptr);
    if (codec_open_rc < 0) {
      debugDump();
      throw std::runtime_error("avcodec_open2");
    }

    parser_ctx = av_parser_init(codec->id);
    check_nullptr(parser_ctx);

    pkt = av_packet_alloc();
    check_nullptr(pkt);

    swr = swr_alloc();
    check_nullptr(swr);

    av_opt_set_int(swr, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(swr, "in_sample_rate", InputInfo::rate, 0);
    av_opt_set_int(swr, "out_sample_rate", InputInfo::rate, 0); // must match for timing
    av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_FORMAT, 0);
    swr_init(swr);
  });
}

bool keep(shRTP rtp, int used) {
  auto m = rtp->_m->data();
  size_t packet_size = rtp->decipher_len + ADTS_HEADER_SIZE;

  if (used < 0) {
    constexpr auto f = FMT_STRING("{} {} failed used={:<6} size={:<6}\n");
    fmt::print(f, runTicks(), fnName(), used, packet_size);
    return false;
  }

  if (used != (int32_t)packet_size) {
    if (true) { // debug
      constexpr auto f = FMT_STRING("{} {} incomplete "
                                    "used={:<6} size={:<6} diff={:+6}\n");
      const int32_t diff = packet_size - used;
      fmt::print(f, runTicks(), fnName(), used, packet_size, diff);
      return false;
    }
  }

  if (pkt->size == 0) {
    constexpr auto f = FMT_STRING("{} {} AAC malformed packet "
                                  "used={} size={}\n");
    fmt::print(f, runTicks(), fnName(), used, packet_size);

    fmt::print("{} decipered: ", runTicks());
    for (size_t idx = 0; idx < packet_size; idx++) {
      if (idx % 5) {
        fmt::print("\n{} decipered: ", runTicks());
      }

      fmt::print("[{:<02}]0x{:<02x} ", idx, m[idx]);
    }

    fmt::print("\n");

    return false;
  }

  if (true) { // debug
    if (pkt->flags != 0) {
      constexpr auto f = FMT_STRING("{} {} AAC packet flags={:#b}\n");
      fmt::print(f, runTicks(), fnName(), pkt->flags);
    }
  }

  return true;
}

uint8_t *mBuffer(shCipherBuff m) {
  // leave space for ADTS header
  return m->data() + ADTS_HEADER_SIZE;
}

void parse(shRTP rtp) {
  if (rtp.use_count() == 1) { // packet is no longer in queue
    return;                   // shared_ptr will be released
  }

  init(); // guarded by call_once
  auto &payload = rtp->payload();
  payload.clear(); // payload will be empty on error

  auto m = rtp->_m.get()->data();
  auto decipher_len = rtp->decipher_len;

  // NOTE: the size of the packet is the deciphered len AND the ADTS header
  const size_t packet_size = decipher_len + ADTS_HEADER_SIZE;

  // make ADTS header
  m[0] = 0xFF;
  m[1] = 0xF9;
  m[2] = ((ADTS_PROFILE - 1) << 6) + (ADTS_FREQ_IDX << 2) + (ADTS_CHANNEL_CFG >> 2);
  m[3] = ((ADTS_CHANNEL_CFG & 3) << 6) + (packet_size >> 11);
  m[4] = (packet_size & 0x7FF) >> 3;
  m[5] = ((packet_size & 7) << 5) + 0x1F;
  m[6] = 0xFC;

  auto used = av_parser_parse2(parser_ctx,     // parser ctx
                               codec_ctx,      // codex ctx
                               &pkt->data,     // ptr to the pkt (parsed data)
                               &pkt->size,     // ptr size of the pkt (parsed data)
                               m,              // payload data (unchanged by parsing)
                               packet_size,    // payload size
                               AV_NOPTS_VALUE, // pts
                               AV_NOPTS_VALUE, // dts
                               0);             // pos

  if (keep(rtp, used) == false) {
    return;
  }

  int ret = 0; // re-used for av* calls
  ret = avcodec_send_packet(codec_ctx, pkt);

  if (ret < 0) {
    constexpr auto f = FMT_STRING("{} {} avcodec_sendpacket={}\n");
    fmt::print(f, runTicks(), fnName(), ret);
    return;
  }

  auto decoded_frame = av_frame_alloc();
  ret = avcodec_receive_frame(codec_ctx, decoded_frame);

  if (ret < 0) {
    constexpr auto f = FMT_STRING("{} {} avcodec_receive_frame={}\n");
    fmt::print(f, runTicks(), fnName(), ret);
    return;
  }

  av_samples_alloc(&pcm_audio,                // pointer to PCM samples
                   &dst_linesize,             // pointer to calculated linesize ?
                   codec_ctx->channels,       // num of channels
                   decoded_frame->nb_samples, // num of samples
                   AV_FORMAT,                 // desired format S16/BE
                   1);                        // no alignment required

  auto samples_per_channel = swr_convert(             // perform the software resample
      swr,                                            // software resample ctx
      &pcm_audio,                                     // put processed samples here
      decoded_frame->nb_samples,                      // output this many samples
      (const uint8_t **)decoded_frame->extended_data, // process data at this ptr
      decoded_frame->nb_samples);                     // process this many samples

  auto dst_bufsize = av_samples_get_buffer_size // determine required buffer
      (&dst_linesize,                           // calc'ed linesize
       codec_ctx->channels,                     // num of channels
       samples_per_channel,                     // num of samples/channel (from swr_convert)
       AV_FORMAT,                               // S16/BE
       1);                                      // no alignment required

  { // debug
    constexpr uint8_t MAX_REPORT = 1;
    static uint8_t count = 0;
    if (count < MAX_REPORT) { // debug
      constexpr auto f = FMT_STRING("{} {} ret={} samples/channel={:<5} dst_buffsize={:<5}\n");
      fmt::print(f, runTicks(), fnName(), ret, samples_per_channel, dst_bufsize);

      ++count;
    }
  }

  if (dst_bufsize > 0) { // put PCM data into payload
    auto to = std::back_inserter(payload);
    ranges::copy_n(pcm_audio, dst_bufsize, to);

    if (true) { // debug
      constexpr int MAX_REPORTS = 5;
      static int count = 0;

      if (count < MAX_REPORTS) {
        constexpr auto f = FMT_STRING("{} {} pcm data={} "
                                      "buf_size={:<5} payload_size={}\n");
        fmt::print(f, runTicks(), fnName(), fmt::ptr(pcm_audio), dst_bufsize, payload.size());

        ++count;
      }
    }

    rtp->decode_ok = true;
  }

  if (decoded_frame) {
    av_frame_free(&decoded_frame);
  }

  if (pcm_audio) {
    av_freep(&pcm_audio);
  }
}

} // namespace av

RTP::RTP(Basic &packet) {
  uint8_t byte0 = packet[0]; // first byte to pick bits from

  version = (byte0 & 0b11000000) >> 6;     // RTPv2 == 0x02
  padding = ((byte0 & 0b00100000) >> 5);   // has padding
  extension = ((byte0 & 0b00010000) >> 4); // has extension
  ssrc_count = ((byte0 & 0b00001111));     // source system record count

  seq_num = rtp::to_uint32(packet.begin() + 1, 3);   // note: onlythree bytes
  timestamp = rtp::to_uint32(packet.begin() + 4, 4); // four bytes
  ssrc = rtp::to_uint32(packet.begin() + 8, 4);      // four bytes

  // grab the end of the packet, we need it for several parts
  Basic::iterator packet_end = packet.begin() + packet.size();

  // nonce is the last eight (8) bytes
  // a complete nonce is 12 bytes so zero pad then append the mini nonce
  _nonce.assign(4, 0x00);
  auto nonce_inserter = std::back_inserter(_nonce);
  ranges::copy_n(packet_end - 8, 8, nonce_inserter);

  // tag is 24 bytes from end and 16 bytes in length
  Basic::iterator tag_begin = packet_end - 24;
  Basic::iterator tag_end = tag_begin + 16;
  _tag.assign(tag_begin, tag_end);

  Basic::iterator aad_begin = packet.begin() + 4;
  Basic::iterator aad_end = aad_begin + 8;
  _aad.assign(aad_begin, aad_end);

  // finally the actual payload, starts at byte 12, ends
  Basic::iterator payload_begin = packet.begin() + 12;
  Basic::iterator payload_end = packet_end - 8;
  _payload.assign(payload_begin, payload_end);

  // NOTE: raw packet falls out of scope
}

// std::swap specialization (friend)
void swap(RTP &a, RTP &b) {
  using std::swap;

  // trivial values
  swap(a.version, b.version);
  swap(a.padding, b.padding);
  swap(a.ssrc_count, b.ssrc_count);
  swap(a.seq_num, b.seq_num);
  swap(a.timestamp, b.timestamp);
  swap(a.ssrc, b.ssrc);
  swap(a.decipher_len, b.decipher_len);
  swap(a.decode_ok, b.decode_ok);

  // containers
  swap(a._m, b._m);
  swap(a._nonce, b._nonce);
  swap(a._tag, b._tag);
  swap(a._aad, b._aad);
  swap(a._payload, b._payload);
}

/*
 * Many thanks to:
 * https://stackoverflow.com/questions/18862715/how-to-generate-the-aac-adts-elementary-stream-with-android-mediacodec
 *
 * Add ADTS header at the beginning of each and every AAC packet.
 * This is needed as MediaCodec encoder generates a packet of raw AAC data.
 *
 * NOTE: the payload size must count in the ADTS header itself.
 */

bool RTP::decipher() {
  if (rtp::shk.empty()) {
    if (true) { // debug
      constexpr auto f = FMT_STRING("{} {} shk empty\n");
      fmt::print(f, runTicks(), fnName());
    }

    return false;
  }

  _m = std::make_shared<CipherBuff>();

  unsigned long long len = 0; // deciphered length
  auto cipher_rc =            // -1 == failure
      crypto_aead_chacha20poly1305_ietf_decrypt(
          av::mBuffer(_m),                   // m (av leaves room for ADTS)
          &len,                              // bytes deciphered
          nullptr,                           // nanoseconds (unused, must be nullptr)
          _payload.data(),                   // ciphered data
          _payload.size(),                   // ciphered length
          _aad.data(),                       // authenticated additional data
          _aad.size(),                       // authenticated additional data length
          _nonce.data(),                     // the nonce
          (const uint8_t *)rtp::shk.data()); // shared key (from SETUP message)

  decipher_len = (size_t)len; // convert chacha len to something more reasonable

  if (cipher_rc < 0) {
    static bool __reported = false; // only report cipher error once

    if (!__reported) {
      constexpr auto f = FMT_STRING("{} {} failed size={} rc={} decipher_len={} \n");
      fmt::print(f, runTicks(), fnName(), _payload.size(), cipher_rc, decipher_len);
      __reported = true;
    }

    return false;
  }

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} OK rc={} decipher/cipher{:>6}/{:>6}\n");
    fmt::print(f, runTicks(), fnName(), cipher_rc, decipher_len, _payload.size());
  }

  return true;
}

void RTP::decode() {
  auto self = shared_from_this();

  av::parse(self);
}

bool RTP::keep(FlushRequest &flush) {
  if (isValid() == false) {
    if (true) { // debug
      constexpr auto f = FMT_STRING("{} {} invalid version=0x{:02x}\n");
      fmt::print(f, runTicks(), moduleId, version);
    }

    return false;
  }

  if (flush.active) {
    if (seq_num < flush.until_seq) { // continue flushing
      return false;                  // don't decipher or keep
    }

    if (seq_num >= flush.until_seq) { // flush complete
      flush.complete();

      if (true) { // debug
        constexpr auto f = FMT_STRING("{} {} FLUSH COMPLETE until_seq={}\n");
        fmt::print(f, runTicks(), moduleId, seq_num);
      }
    }
  }

  return decipher();
}

// misc debug

void RTP::dump(bool debug) const {
  if (!debug) {
    return;
  }

  constexpr auto f = FMT_STRING("{} {} vsn={} pad={} ext={} ssrc_count={} "
                                "payload_size={:>4} seq_num={:>8} ts={:>12}\n");
  fmt::print(f, runTicks(), moduleId, version, padding, extension, ssrc_count, payloadSize(),
             seq_num, timestamp);
}

// class static data

void RTP::shk(const Basic &key) {
  rtp::shk = key;

  if (false) { // debug
    constexpr auto f = FMT_STRING("{} {} size={}\n");
    fmt::print(f, runTicks(), fnName(), rtp::shk.size());
  }
}

void RTP::shkClear() { rtp::shk.clear(); }

} // namespace packet
} // namespace pierre