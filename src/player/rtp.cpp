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

#include "player/rtp.hpp"
#include "core/input_info.hpp"
#include "packet/basic.hpp"
#include "player/av.hpp"
#include "player/fft.hpp"
#include "player/peaks.hpp"
#include "rtp_time/anchor.hpp"

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
#include <vector>

namespace pierre {

namespace fra {
StateKeys &stateKeys() {
  static auto keys = StateKeys{DECODED, OUTDATED, PLAYABLE, PLAYED, FUTURE};

  return keys;
}
} // namespace fra

namespace player {

namespace chrono = std::chrono;
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

constexpr auto AV_FORMAT_IN = AV_SAMPLE_FMT_FLTP;
constexpr auto AV_FORMAT_OUT = AV_SAMPLE_FMT_S16;
constexpr auto AV_CH_LAYOUT = AV_CH_LAYOUT_STEREO;

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

    av_opt_set_channel_layout(swr, "in_channel_layout", AV_CH_LAYOUT, 0);
    av_opt_set_channel_layout(swr, "out_channel_layout", AV_CH_LAYOUT, 0);
    av_opt_set_int(swr, "in_sample_rate", InputInfo::rate, 0);
    av_opt_set_int(swr, "out_sample_rate", InputInfo::rate, 0); // must match for timing
    av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_FORMAT_IN, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_FORMAT_OUT, 0);
    swr_init(swr);
  });
}

bool keep(shRTP rtp, int used) {
  auto m = rtp->_m->data();
  size_t packet_size = rtp->decipher_len + ADTS_HEADER_SIZE;

  if (used < 0) {
    __LOG0("{} failed used={:<6} size={:<6}\n", fnName(), used, packet_size);
    return false;
  }

  if (used != (int32_t)packet_size) {
    __LOG0("{} incomplete used={:<6} size={:<6} diff={:+6}\n", //
           fnName(), used, packet_size, int32_t(packet_size - used));
    return false;
  }

  if (pkt->size == 0) {
    __LOG0("{} AAC malformed packet used={} size={}\n", fnName(), used, packet_size);

    string msg;
    auto w = std::back_inserter(msg);

    for (size_t idx = 0; idx < packet_size; idx++) {
      if (idx % 5) {
        fmt::format_to(w, "\n{} ", __LOG_PREFIX);
      }

      fmt::format_to(w, "[{:<02}]0x{:<02x} ", idx, m[idx]);
    }

    __LOG0("{}\n", msg);

    return false;
  }

  return true;
}

uint8_t *mBuffer(shCipherBuff m) {
  // leave space for ADTS header
  return m->data() + ADTS_HEADER_SIZE;
}

void parse(shRTP rtp) {
  if (rtp.use_count() == 1) { // packet is no longer in queue
    if (true) {               // debug
      constexpr auto f = FMT_STRING("{} {} rtp_packet not queued, skipping decode\n");
      fmt::print(f, runTicks(), fnName());
    }
    return;
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
    __LOG0("{} AV send packet failed decipher_len={} size={} flags={:#b} rc=0x{:x}\n", //
           fnName(), decipher_len, pkt->size, pkt->flags, ret);
    return;
  }

  auto decoded_frame = av_frame_alloc();
  ret = avcodec_receive_frame(codec_ctx, decoded_frame);

  if (ret != 0) {
    __LOG0("{} avcodec_receive_frame={}\n", fnName(), ret);
    return;
  }

  rtp->channels = codec_ctx->channels;

  av_samples_alloc(&pcm_audio,                // pointer to PCM samples
                   &dst_linesize,             // pointer to calculated linesize ?
                   rtp->channels,             // num of channels
                   decoded_frame->nb_samples, // num of samples
                   AV_FORMAT_OUT,             // desired format (see const above)
                   0);                        // default alignment

  rtp->samples_per_channel = swr_convert(             // perform the software resample
      swr,                                            // software resample ctx
      &pcm_audio,                                     // put processed samples here
      decoded_frame->nb_samples,                      // output this many samples
      (const uint8_t **)decoded_frame->extended_data, // process data at this ptr
      decoded_frame->nb_samples);                     // process this many samples

  auto dst_bufsize = av_samples_get_buffer_size // determine required buffer
      (&dst_linesize,                           // calc'ed linesize
       rtp->channels,                           // num of channels
       rtp->samples_per_channel,                // num of samples/channel (from swr_convert)
       AV_FORMAT_OUT,                           // desired format (see const above)
       1);                                      // default alignment

  { // debug
    constexpr uint8_t MAX_REPORT = 1;
    static uint8_t count = 0;
    if (count < MAX_REPORT) {                                                    // debug
      __LOG0("{} ret={} channels={} samples/channel={:<5} dst_buffsize={:<5}\n", //
             fnName(), ret, rtp->channels, rtp->samples_per_channel, dst_bufsize);

      ++count;
    }
  }

  if (dst_bufsize > 0) { // put PCM data into payload
    auto to = std::back_inserter(payload);
    ranges::copy_n(pcm_audio, dst_bufsize, to);

    if (true) { // debug
      constexpr int MAX_REPORTS = 1;
      static int count = 0;

      if (count < MAX_REPORTS) {
        __LOG0("{} pcm data={} buf_size={:<5} payload_size={}\n", //
               fnName(), fmt::ptr(pcm_audio), dst_bufsize, payload.size());

        ++count;
      }
    }

    rtp->decodeOk();
  }

  if (decoded_frame) {
    av_frame_free(&decoded_frame);
  }

  if (pcm_audio) {
    av_freep(&pcm_audio);
  }
}

} // namespace av

RTP::RTP(packet::Basic &packet) {
  uint8_t byte0 = packet[0]; // first byte to pick bits from

  version = (byte0 & 0b11000000) >> 6;     // RTPv2 == 0x02
  padding = ((byte0 & 0b00100000) >> 5);   // has padding
  extension = ((byte0 & 0b00010000) >> 4); // has extension
  ssrc_count = ((byte0 & 0b00001111));     // source system record count

  seq_num = rtp::to_uint32(packet.begin() + 1, 3);   // note: onlythree bytes
  timestamp = rtp::to_uint32(packet.begin() + 4, 4); // four bytes
  ssrc = rtp::to_uint32(packet.begin() + 8, 4);      // four bytes

  // grab the end of the packet, we need it for several parts
  packet::Basic::iterator packet_end = packet.begin() + packet.size();

  // nonce is the last eight (8) bytes
  // a complete nonce is 12 bytes so zero pad then append the mini nonce
  _nonce.assign(4, 0x00);
  auto nonce_inserter = std::back_inserter(_nonce);
  ranges::copy_n(packet_end - 8, 8, nonce_inserter);

  // tag is 24 bytes from end and 16 bytes in length
  packet::Basic::iterator tag_begin = packet_end - 24;
  packet::Basic::iterator tag_end = tag_begin + 16;
  _tag.assign(tag_begin, tag_end);

  packet::Basic::iterator aad_begin = packet.begin() + 4;
  packet::Basic::iterator aad_end = aad_begin + 8;
  _aad.assign(aad_begin, aad_end);

  // finally the actual payload, starts at byte 12, ends
  packet::Basic::iterator payload_begin = packet.begin() + 12;
  packet::Basic::iterator payload_end = packet_end - 8;
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
  swap(a.samples_per_channel, b.samples_per_channel);
  swap(a.channels, b.channels);
  swap(a._state, b._state);

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
 * NOTE: the payload size must include the ADTS header size
 */

// static functions to create a new RTP

shRTP RTP::create(packet::Basic &packet) { // static
  return std::shared_ptr<RTP>(new RTP(packet));
}

shRTP RTP::create(csv type) { // static
  packet::Basic empty;
  empty.assign(PAYLOAD_MIN_SIZE, 0x00);

  if (type == player::SILENCE) {
    auto frame = create(empty);
    frame->_state = fra::PLAYABLE;
    frame->local_time_diff = Nanos(0);
    return frame;
  }

  return shRTP();
}

// genereal API and member functions

size_t RTP::available(const fra::StatsMap &stats_map) { // static
  static const fra::StateKeys want_states{fra::DECODED, fra::PLAYABLE, fra::FUTURE};

  size_t count = 0;
  ranges::for_each(want_states, [&](const auto &state) { count += stats_map.at(state); });

  return count;
}

bool RTP::empty(shRTP frame) { // static
  auto rc = true;
  if (frame.use_count()) {
    rc = frame->_state == fra::EMPTY;
  }

  return rc;
}

bool RTP::decipher() {
  if (rtp::shk.empty()) {
    __LOG0("{} shk empty\n", fnName());

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

    const auto reason = (decipher_len == 0) ? csv("EMPTY") : csv("ERROR");

    if (!__reported) {
      __LOG0("{} size={} rc={} decipher_len={} \n", //
             fnName(), reason, _payload.size(), cipher_rc, decipher_len);
      __reported = true;
    }

    return false;
  }

  __LOGX("{} OK rc={} decipher/cipher{:>6}/{:>6}\n", //
         fnName(), cipher_rc, decipher_len, _payload.size());

  return true;
}

void RTP::decode(shRTP rtp_packet) {
  // function definition must be in this file due to deliberate limited visibility of
  // av namespace
  av::parse(rtp_packet);
}

void RTP::findPeaks(shRTP rtp_packet) {
  auto &payload = rtp_packet->_payload;
  const auto samples_per_channel = rtp_packet->samples_per_channel;

  FFT fft_left(samples_per_channel, InputInfo::rate);
  FFT fft_right(samples_per_channel, InputInfo::rate);

  // first things first, deinterlace the raw *uint8_t) payload into left/right reals (floats)
  auto &left = fft_left.real();
  auto &right = fft_right.real();

  constexpr size_t SAMPLE_BYTES = sizeof(uint16_t); // bytes per sample
  uint8_t *fptr = payload.data();                   // ptr into array of uint8_t to convert
  const auto interleaved_samples = payload.size() / SAMPLE_BYTES;

  for (size_t idx = 0; idx < interleaved_samples; ++idx) {
    uint16_t val;
    std::memcpy(&val, fptr, SAMPLE_BYTES); // memcpy to avoid strict aliasing rules
    fptr += SAMPLE_BYTES;                  // point to the next sample

    if ((idx % 2) == 0) { // left channel
      left.emplace_back(static_cast<float>(val));
    } else { // right channel
      right.emplace_back(static_cast<float>(val));
    }
  }

  // now process each channel then save the peaks
  fft_left.process();
  fft_right.process();

  rtp_packet->_peaks_left = fft_left.findPeaks();
  rtp_packet->_peaks_right = fft_right.findPeaks();

  // clear the payload
  packet::Basic empty;
  std::swap(empty, payload);
}

bool RTP::keep(FlushRequest &flush) {
  if (isValid() == false) {
    __LOG0("{} invalid version=0x{:02x}\n", moduleId, version);

    return false;
  }

  if (flush.active) {
    if (seq_num <= flush.until_seq) { // continue flushing
      return false;                   // don't decipher or keep
    }

    if (seq_num > flush.until_seq) { // flush complete
      flush.complete();

      __LOG0("{} FLUSH COMPLETE until_seq={}\n", moduleId, seq_num);
    }
  }

  return (decipher() && (decipher_len > 0));
}

const Nanos RTP::localTimeDiff() const {
  const auto &anchor_data = Anchor::getData();
  const auto net_time_ns = anchor_data.netTimeNow();
  const auto frame_time_ns = anchor_data.frameLocalTime(timestamp);

  if (anchor_data.ok()) {
    return chrono::duration_cast<Nanos>(frame_time_ns - net_time_ns);
  } else {
    return Nanos::min();
  }
}

bool RTP::playable(shRTP frame) { // static
  return (frame.use_count() && (frame->_state == fra::PLAYABLE));
}

bool RTP::stateEqual(const fra::States &states) const {
  return ranges::any_of(states, [&](const auto &state) { return stateEqual(state); });
}

shRTP RTP::stateNow(const Nanos &lead_ns) {
  if (timestamp && seq_num) { // frame is legit
    const auto diff = localTimeDiff();

    if (diff == Nanos::min()) { // anchor data not ready
      return create(SILENCE);
    }

    // handle decoded, playable and future frames
    if (stateEqual({fra::DECODED, fra::PLAYABLE, fra::FUTURE})) {
      local_time_diff = diff;

      // note: state unchanged when if statements are false
      if (diff < Nanos::zero()) { // too old
        _state = fra::OUTDATED;
        __LOGX("{} state={:<10} seq_num={:<8} diff={}\n", moduleId, _state, seq_num,
               chrono::duration_cast<MillisFP>(local_time_diff));

      } else if (diff <= lead_ns) { // ready to play
        _state = fra::PLAYABLE;
      } else if (diff > lead_ns) { // time to play missed

        _state = fra::FUTURE;
        __LOGX("{} state={:<10} seq_num={:<8} diff={}\n", moduleId, _state, seq_num,
               chrono::duration_cast<MillisFP>(local_time_diff));
      }
    }
  }
  return shared_from_this();
}

fra::StateConst &RTP::stateVal(shRTP frame) { // static

  if (frame.use_count()) {
    return frame->_state;
  }

  return fra::EMPTY;
}

fra::StatsMap RTP::statsMap() { // static
  fra::StatsMap stats_map;

  std::ranges::for_each(fra::stateKeys(), [&](auto state) { //
    stats_map.try_emplace(state, 0);
  });

  return stats_map;
}

// misc debug

void RTP::dump(bool debug) const {
  if (!debug) {
    return;
  }

  __LOG0("{} vsn={} pad={} ext={} ssrc_count={} payload_size={:>4} seq_num={:>8} ts={:>12}\n",
         moduleId, version, padding, extension, ssrc_count, payloadSize(), seq_num, timestamp);
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

} // namespace player
} // namespace pierre