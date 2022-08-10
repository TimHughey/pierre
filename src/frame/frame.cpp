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

#include "frame/frame.hpp"
#include "base/elapsed.hpp"
#include "base/input_info.hpp"
#include "base/shk.hpp"
#include "base/uint8v.hpp"
#include "dsp/fft.hpp"
#include "dsp/peaks.hpp"
#include "frame/av.hpp"
#include "rtp_time/anchor.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <iterator>
#include <mutex>
#include <ranges>
#include <sodium.h>
#include <utility>
#include <vector>

namespace chrono = std::chrono;
namespace ranges = std::ranges;

namespace pierre {
namespace fra {
StateKeys &stateKeys() {
  static auto keys = StateKeys{DECODED, OUTDATED, PLAYABLE, PLAYED, FUTURE};

  return keys;
}
} // namespace fra

namespace av { // encapsulation of libav*

// forward decls
void check_nullptr(void *ptr);
void debugDump();
void init(); // throws on alloc failures
uint8_t *mBuffer();
void parse(shFrame frame);
bool keep(shFrame frame, int used);
constexpr auto moduleId = csv("av::FRAME");

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
  string msg;
  auto w = std::back_inserter(msg);

  constexpr auto f = FMT_STRING("{:>25}={:}\n");
  fmt::format_to(w, f, "AVCodec", fmt::ptr(codec));
  fmt::format_to(w, f, "AVCodecContext", fmt::ptr(codec_ctx));
  fmt::format_to(w, f, "codec_open_rc", codec_open_rc);
  fmt::format_to(w, f, "AVCodecParserContext", fmt::ptr(parser_ctx));
  fmt::format_to(w, f, "AVPacket", fmt::ptr(pkt));
  fmt::format_to(w, f, "SwrContext", fmt::ptr(swr));

  __LOG0(LCOL01 "\n{}\n", moduleId, csv("DEBUG DUMP"), msg);
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

bool keep(shFrame frame, int used) {
  auto rc = true; // keep frame unless checks fail
  auto m = frame->_m->data();
  int32_t packet_size = frame->decipher_len + ADTS_HEADER_SIZE;

  if ((used < 0) || (used != packet_size) || (pkt->size == 0)) {
    rc = false;

    constexpr auto decipher = csv("DECIPHER FAILED");
    constexpr auto malformed = csv("AAC MALFORMED");

    string msg;
    auto w = std::back_inserter(msg);
    fmt::format_to(w, "{:<18}", moduleId);
    fmt::format_to(w, " {} used={:<6} size={:<6}",        // fmt
                   pkt->size == 0 ? malformed : decipher, // log reason
                   used, packet_size);

    // add used vs. packet size (if needed)
    if (int32_t diff = packet_size - used; diff && (used > 0) && pkt->size) {
      fmt::format_to(w, " diff={:+6}", diff);
    }

    if (pkt->size == 0) { // AAC malformed, dump packet bytes
      for (auto idx = 0; idx < packet_size; idx++) {
        if ((idx % 5) || (idx == 0)) { // new bytes row
          fmt::format_to(w, "\n{} ", __LOG_PREFIX);
        }

        fmt::format_to(w, "[{:<02}]0x{:<02x} ", idx, m[idx]);
      }
    }

    __LOG0("{}\n", msg);
  } // end of debug logging

  return rc;
}

uint8_t *mBuffer(shCipherBuff m) {
  // leave space for ADTS header
  return m->data() + ADTS_HEADER_SIZE;
}

void parse(shFrame frame) {
  if (frame.use_count() == 1) { // packet is no longer in queue
    __LOG0("{:<18} not queued, skipping decode\n", moduleId);
    return;
  }

  init(); // guarded by call_once
  auto &payload = frame->payload();
  payload.clear(); // payload will be empty on error

  auto m = frame->_m.get()->data();
  auto decipher_len = frame->decipher_len;

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

  if (keep(frame, used) == false) {
    return;
  }

  int ret = 0; // re-used for av* calls
  ret = avcodec_send_packet(codec_ctx, pkt);

  if (ret < 0) {
    __LOG0("{:<18} AV send packet failed decipher_len={} size={} flags={:#b} rc=0x{:x}\n", //
           moduleId, decipher_len, pkt->size, pkt->flags, ret);
    return;
  }

  auto decoded_frame = av_frame_alloc();
  ret = avcodec_receive_frame(codec_ctx, decoded_frame);

  if (ret != 0) {
    __LOG0("{:<18} avcodec_receive_frame={}\n", moduleId, ret);
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

  { // debug
    constexpr uint8_t MAX_REPORT = 1;
    static uint8_t count = 0;
    if (count < MAX_REPORT) {
      __LOG0("{:<18} ret={} channels={} samples/channel={:<5} dst_buffsize={:<5}\n", //
             moduleId, ret, frame->channels, frame->samples_per_channel, dst_bufsize);

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
        __LOG0("{:<18} pcm data={} buf_size={:<5} payload_size={}\n", //
               moduleId, fmt::ptr(pcm_audio), dst_bufsize, payload.size());

        ++count;
      }
    }

    frame->decodeOk();
  }

  if (decoded_frame) {
    av_frame_free(&decoded_frame);
  }

  if (pcm_audio) {
    av_freep(&pcm_audio);
  }
}

} // namespace av

// frame helpers
uint32_t to_uint32(uint8v::iterator begin, long int count) {
  uint32_t val = 0;
  size_t shift = (count - 1) * 8;

  for (auto it = std::counted_iterator{begin, count}; it != std::default_sentinel;
       ++it, shift -= 8) {
    val += *it << shift;
  }

  return val;
}

Frame::Frame(uint8v &packet) {
  uint8_t byte0 = packet[0]; // first byte to pick bits from

  version = (byte0 & 0b11000000) >> 6;     // RTPv2 == 0x02
  padding = ((byte0 & 0b00100000) >> 5);   // has padding
  extension = ((byte0 & 0b00010000) >> 4); // has extension
  ssrc_count = ((byte0 & 0b00001111));     // source system record count

  seq_num = to_uint32(packet.begin() + 1, 3);   // note: only three bytes
  timestamp = to_uint32(packet.begin() + 4, 4); // four bytes
  ssrc = to_uint32(packet.begin() + 8, 4);      // four bytes

  // grab the end of the packet, we need it for several parts
  uint8v::iterator packet_end = packet.begin() + packet.size();

  // nonce is the last eight (8) bytes
  // a complete nonce is 12 bytes so zero pad then append the mini nonce
  _nonce.assign(4, 0x00);
  ranges::copy_n(packet_end - 8, 8, std::back_inserter(_nonce));

  // tag is 24 bytes from end and 16 bytes in length
  uint8v::iterator tag_begin = packet_end - 24;
  uint8v::iterator tag_end = tag_begin + 16;
  _tag.assign(tag_begin, tag_end);

  uint8v::iterator aad_begin = packet.begin() + 4;
  uint8v::iterator aad_end = aad_begin + 8;
  _aad.assign(aad_begin, aad_end);

  // finally the actual payload, starts at byte 12, ends
  uint8v::iterator payload_begin = packet.begin() + 12;
  uint8v::iterator payload_end = packet_end - 8;
  _payload.assign(payload_begin, payload_end);

  // NOTE: raw packet falls out of scope
}

// std::swap specialization (friend)
void swap(Frame &a, Frame &b) {
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

// general API and member functions

bool Frame::decipher() {
  if (SharedKey::empty()) {
    __LOG0("{:<18} shared key empty\n", moduleId);

    return false;
  }

  _m = std::make_shared<CipherBuff>();

  auto cipher_rc = // -1 == failure
      crypto_aead_chacha20poly1305_ietf_decrypt(
          av::mBuffer(_m),   // m (av leaves room for ADTS)
          &decipher_len,     // bytes deciphered
          nullptr,           // nanoseconds (unused, must be nullptr)
          _payload.data(),   // ciphered data
          _payload.size(),   // ciphered length
          _aad.data(),       // authenticated additional data
          _aad.size(),       // authenticated additional data length
          _nonce.data(),     // the nonce
          SharedKey::key()); // shared key (from SETUP message)

  if (cipher_rc < 0) {
    static bool __reported = false; // only report cipher error once

    if (!__reported) {
      __LOG0("{:<18} decipher size={} rc={} decipher_len={} \n", //
             moduleId, _payload.size(), cipher_rc, decipher_len == 0 ? fra::EMPTY : fra::ERROR);
      __reported = true;
    }

    return false;
  }

  __LOGX("{:<18} decipher OK rc={} decipher/cipher{:>6}/{:>6}\n", //
         moduleId, cipher_rc, decipher_len, _payload.size());

  return true;
}

void Frame::findPeaks(shFrame frame) {
  auto &payload = frame->_payload;
  const auto samples_per_channel = frame->samples_per_channel;

  FFT fft_left(samples_per_channel, InputInfo::rate);
  FFT fft_right(samples_per_channel, InputInfo::rate);

  // first things first, deinterlace the raw *uint8_t) payload into left/right reals (floats)
  auto &left = fft_left.real();
  auto &right = fft_right.real();

  constexpr size_t SAMPLE_BYTES = sizeof(uint16_t); // bytes per sample
  uint8_t *sptr = payload.data();                   // ptr to next sample
  const auto interleaved_samples = payload.size() / SAMPLE_BYTES;

  for (size_t idx = 0; idx < interleaved_samples; ++idx) {
    uint16_t val;
    std::memcpy(&val, sptr, SAMPLE_BYTES); // memcpy to avoid strict aliasing rules
    sptr += SAMPLE_BYTES;                  // increment to next sample

    if ((idx % 2) == 0) { // left channel
      left.emplace_back(static_cast<float>(val));
    } else { // right channel
      right.emplace_back(static_cast<float>(val));
    }
  }

  // now process each channel then save the peaks
  fft_left.process();
  fft_right.process();

  frame->_peaks_left = fft_left.findPeaks();
  frame->_peaks_right = fft_right.findPeaks();

  frame->_silence = Peaks::silence(frame->_peaks_left) && Peaks::silence(frame->_peaks_right);

  // really clear the payload (prevent lingering memory allocations)
  uint8v empty;
  std::swap(empty, payload);
}

bool Frame::keep(FlushRequest &flush) {
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

  auto rc = false;
  if (decipher(); decipher_len) {
    rc = true;
    av::parse(this->shared_from_this());
  }

  return rc;
}

const Nanos Frame::localTimeDiff() {
  // must be in .cpp due to intentional limiting of visibility of Anchor
  if (const auto &data = Anchor::getData(); data.ok()) {
    anchor_data = data;

    return pe_time::elapsed_as<Nanos>(data.netTimeNow(), data.frameLocalTime(timestamp));
  }

  return Nanos::min();
}

bool Frame::nextFrame(const Nanos &lead_time) {
  auto rc = true;               // true: stop searching, false: keep searching
  const bool ready = isReady(); // cache value in case it changes during this function

  if (timestamp && seq_num && ready) {                             // frame is legit
    if (const auto diff = localTimeDiff(); diff != Nanos::min()) { // anchor is ready

      // determine if the state of the frame should be changed based on local time diff
      // we only want to look at specific frame states
      if (rc = unplayed(); rc == true) {
        local_time_diff = diff; // set local_time_diff for debugging

        if (diff < Nanos::zero()) { // old, before required lead time
          _state = fra::OUTDATED;   // mark as outdated
          rc = false;               // keep searching

        } else if (diff <= lead_time) { // within lead time range
          _state = fra::PLAYABLE;       // mark as playable

        } else if ((diff > lead_time) && (_state == fra::DECODED)) { // future frame
          _state = fra::FUTURE;
        }

        __LOGX(LCOL01 " {}\n", moduleId, csv("NEXT_FRAME"), inspect());
      } // end frame state update
    }   // anchor not ready
  }     // frame failed valid or ready...

  // NOTE:
  // return value designed for use with ranges::find_if: true == found; false == not found
  // the caller will decide how to use this frame,  for example: play if playable, if late
  // potentially quickly call nextFrame() to determine if this frame should play or
  // the time diff can be used to correct an out-of-sync condition

  return rc;
}

PeakInfo Frame::peakInfo(Elapsed &uptime) {
  auto adata = anchor::Data::any_cast(anchor_data);

  return PeakInfo{
      .seq_num = seq_num,                                 // frame seq_num
      .timestamp = timestamp,                             // frame timestamp (raw)
      .left = _peaks_left,                                // left channel peaks
      .right = _peaks_right,                              // right channel peaks
      .silence = _silence,                                // frame is silence
      .nettime_now = adata.netTimeNow(),                  // current network time (master clock)
      .frame_localtime = adata.frameLocalTime(timestamp), // frame calculated local time
      .uptime = uptime                                    // uptime of render thread
  };
}

// misc debug
const string Frame::inspectFrame(shFrame frame, bool full) { // static
  string msg;
  auto w = std::back_inserter(msg);

  if (frame.use_count() == 0) { // shared ptr is empty
    fmt::format_to(w, " <empty>");
  } else {
    if (full == true) {
      fmt::format_to(w, " vsn={} pad={} ext={} ssrc={} size={:>4}", //
                     frame->version, frame->padding, frame->extension, frame->ssrc_count,
                     frame->payloadSize());
    }

    fmt::format_to(w, " seq_num={:<8} ts={:<12} state={:<10} use_count={} ready={}", //
                   frame->seq_num, frame->timestamp, frame->_state, frame.use_count(),
                   frame->isReady());

    if (frame->local_time_diff != Nanos::min()) {
      fmt::format_to(w, " diff={:7.2}", pe_time::as_millis_fp(frame->local_time_diff));
    }

    if (frame->silence()) {
      fmt::format_to(w, " silence=true");
    }
  }

  return msg;
}

} // namespace pierre
