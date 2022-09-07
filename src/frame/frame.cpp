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
#include "base/threads.hpp"
#include "base/uint8v.hpp"
#include "config/config.hpp"
#include "frame/av.hpp"
#include "frame/fft.hpp"
#include "frame/peaks.hpp"
#include "io/io.hpp"
#include "rtp_time/anchor.hpp"

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
constexpr auto module_id = csv("av::FRAME");

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

  __LOG0(LCOL01 "\n{}\n", module_id, csv("DEBUG DUMP"), msg);
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
    fmt::format_to(w, "{:<18}", module_id);
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
    __LOG0("{:<18} not queued, skipping decode\n", module_id);
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

  if (dst_bufsize > 0) { // put PCM data into payload
    auto to = std::back_inserter(payload);
    ranges::copy_n(pcm_audio, dst_bufsize, to);

    __LOGX(LCOL01 " pcm data={} buf_size={:<5} payload_size={}\n", //
           module_id, "INFO", fmt::ptr(pcm_audio), dst_bufsize, payload.size());

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

/*
 * Many thanks to:
 * https://stackoverflow.com/questions/18862715/how-to-generate-the-aac-adts-elementary-stream-with-android-mediacodec
 *
 * Add ADTS header at the beginning of each and every AAC packet.
 * This is needed as MediaCodec encoder generates a packet of raw AAC data.
 *
 * NOTE: the payload size must include the ADTS header size
 */

// Digital Signal Analysis Async Threads
namespace dsp {
// order dependent
static io_context io_ctx;                   // digital signal analysis io_ctx
static steady_timer watchdog_timer(io_ctx); // watch for shutdown
static work_guard guard(io_ctx.get_executor());
static constexpr csv moduleID() { return csv{"DSP"}; }

// order independent
static Thread thread_main;
static Threads threads;
static std::stop_token stop_token;
static std::once_flag once_flag;

// forward decls
void init();
void process(shFrame frame);
void watch_dog();
void shutdown();

// initialize the thread pool for digital signal analysis
void init() {
  float dsp_hw_factor = Config::object("dsp")["concurrency_factor"];
  int thread_count = std::jthread::hardware_concurrency() * dsp_hw_factor;

  std::latch latch{thread_count};

  for (auto n = 0; n < thread_count; n++) {
    // notes:
    //  1. start DSP processing threads
    threads.emplace_back([=, &latch](std::stop_token token) {
      std::call_once(once_flag, [&token]() mutable { stop_token = token; });

      name_thread("Frame", n);
      latch.count_down();
      io_ctx.run(); // dsp (frame) io_ctx
    });
  }

  watch_dog(); // watch for shutdown

  // caller thread waits until all threads are started
  latch.wait();
}

// perform digital signal analysis on a Frame
void process(shFrame frame) {
  asio::post(io_ctx, [=]() { frame->findPeaks(); });
}

// shutdown thread pool and wait for all threads to stop
void shutdown() {
  thread_main.request_stop();
  thread_main.join();
  ranges::for_each(threads, [](auto &t) { t.join(); });
}

// watch for thread stop request
void watch_dog() {
  // cancels any running timers
  [[maybe_unused]] auto canceled = watchdog_timer.expires_after(250ms);

  watchdog_timer.async_wait([&](const error_code ec) {
    if (ec == errc::success) { // unless success, fall out of scape

      // check if any thread has received a stop request
      if (stop_token.stop_requested()) {
        io_ctx.stop();
      } else {
        watch_dog();
      }
    } else {
      __LOG0(LCOL01 " going out of scope reason={}\n", //
             moduleID(), csv("WATCH_DOG"), ec.message());
    }
  });
}

} // namespace dsp

// BEGIN FRAME IMPLEMENTATION
Frame::Frame(uint8v &packet) : lead_time{0} {
  uint8_t byte0 = packet[0]; // first byte to pick bits from

  version = (byte0 & 0b11000000) >> 6;     // RTPv2 == 0x02
  padding = ((byte0 & 0b00100000) >> 5);   // has padding
  extension = ((byte0 & 0b00010000) >> 4); // has extension
  ssrc_count = ((byte0 & 0b00001111));     // source system record count

  seq_num = packet.to_uint32(1, 3);   // note: only three bytes
  timestamp = packet.to_uint32(4, 4); // four bytes
  ssrc = packet.to_uint32(8, 4);      // four bytes

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

Frame::Frame(const Nanos lead_time, fra::State state)
    : version(0x02),                // ensure version is valid
      lead_time{lead_time},         // used to simulate anchor
      state(state),                 // populate state (typically fra::DECODED)
      peaks_left(Peaks::create()),  // ensure left peaks exist (and represent silence)
      peaks_right(Peaks::create()), // ensure right peaks exist (and represent silence)
      use_anchor(false)             // do not use Anchor / MasterClock for frame timing
{
  if (playable()) {
    sync_wait = local_time_diff();
  }
}

// Frame API and member functions
bool Frame::decipher() {
  if (SharedKey::empty()) {
    __LOG0("{:<18} shared key empty\n", module_id);

    return false;
  }

  _m = std::make_shared<CipherBuff>(); // TODO - release or make automatic

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
             module_id, _payload.size(), cipher_rc, decipher_len == 0 ? fra::EMPTY : fra::ERROR);
      __reported = true;
    }

    return false;
  }

  __LOGX("{:<18} decipher OK rc={} decipher/cipher{:>6}/{:>6}\n", //
         module_id, cipher_rc, decipher_len, _payload.size());

  return true;
}

void Frame::findPeaks() {
  constexpr size_t SAMPLE_BYTES = sizeof(uint16_t); // bytes per sample

  FFT fft_left(samples_per_channel, InputInfo::rate);
  FFT fft_right(samples_per_channel, InputInfo::rate);

  // first things first, deinterlace the raw *uint8_t) payload into left/right reals (floats)
  auto &left = fft_left.real();
  auto &right = fft_right.real();

  uint8_t *sptr = _payload.data(); // ptr to next sample
  const auto interleaved_samples = _payload.size() / SAMPLE_BYTES;

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

  peaks_left = fft_left.findPeaks();
  peaks_right = fft_right.findPeaks();

  // really clear the payload (prevent lingering memory allocations)
  uint8v empty;
  std::swap(empty, _payload);
}

bool Frame::keep(FlushRequest &flush) {
  if (isValid() == false) {
    __LOG0("{} invalid version=0x{:02x}\n", module_id, version);

    return false;
  }

  if (flush.active) {
    if (seq_num <= flush.until_seq) { // continue flushing
      return false;                   // don't decipher or keep
    }

    if (seq_num > flush.until_seq) { // flush complete
      flush.complete();

      __LOG0("{} FLUSH COMPLETE until_seq={}\n", module_id, seq_num);
    }
  }

  auto rc = false;
  if (decipher(); decipher_len) {
    rc = true;
    av::parse(shared_from_this());
    dsp::process(shared_from_this());
  }

  return rc;
}

const Nanos Frame::local_time_diff() {
  // must be in .cpp due to intentional limiting of Anchor visibility

  return use_anchor ? Anchor::frame_diff(timestamp)                   // calc via master clock
                    : lead_time - pet::elapsed_as<Nanos>(created_at); // simulate a clock
}

shFrame Frame::state_now(const Nanos &lead_time) {
  if (isReady() && unplayed()) { // frame is legit
    sync_wait = local_time_diff();

    if (sync_wait > Nanos::min()) { // anchor is ready or not using anchor

      // determine if the state of the frame should be changed based on local time sync_wait
      // we only want to look at specific frame states
      if (unplayed()) {
        if (sync_wait < Nanos::zero()) { // old, before required lead time
          state = fra::OUTDATED;         // mark as outdated

        } else if ((sync_wait >= Nanos::zero()) && (sync_wait <= lead_time)) { // in range
          state = fra::PLAYABLE;

        } else if ((sync_wait > lead_time) && (state == fra::DECODED)) { // future
          state = fra::FUTURE;
        }

        __LOGX(LCOL01 " {}\n", module_id, csv("STATE_NOW"), inspect());
      } // end frame state update
    }   // anchor not ready
  }

  return shared_from_this();
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
                   frame->seq_num, frame->timestamp, frame->state, frame.use_count(),
                   frame->isReady());

    if (frame->sync_wait > Nanos::min()) {
      fmt::format_to(w, " sync_wait={:7.2}", pet::as_millis_fp(frame->sync_wait));
    }

    if (frame->silent()) {
      fmt::format_to(w, " silence=true");
    }
  }

  return msg;
}

} // namespace pierre
