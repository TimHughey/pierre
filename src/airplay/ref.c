// default buffer size
// needs to be a power of 2 because of the way BUFIDX(seqno) works
#define BUFFER_FRAMES 1024
#define MAX_PACKET 2048

abuf_t audio_buffer[BUFFER_FRAMES];

#define STANDARD_PACKET_SIZE 4096

// DAC buffer occupancy stuff
#define DAC_BUFFER_QUEUE_MINIMUM_LENGTH 2500

// static abuf_t audio_buffer[BUFFER_FRAMES];
#define BUFIDX(seqno) ((seq_t)(seqno) % BUFFER_FRAMES)

#define SIGN_EXTENDED32(val, bits) ((val << (32 - bits)) >> (32 - bits))

struct {
  signed int x : 24;
} se_struct_24;
#define SignExtend24(val) (se_struct_24.x = val)

#define _Swap16(v)                                                                                \
  do {                                                                                            \
    v = (((v)&0x00FF) << 0x08) | (((v)&0xFF00) >> 0x08);                                          \
  } while (0)

static void ab_resync(rtsp_conn_info *conn) {
  int i;
  for (i = 0; i < BUFFER_FRAMES; i++) {
    conn->audio_buffer[i].ready = 0;
    conn->audio_buffer[i].resend_request_number = 0;
    conn->audio_buffer[i].resend_time =
        0; // this is either zero or the time the last resend was requested.
    conn->audio_buffer[i].initialisation_time =
        0; // this is either the time the packet was received or the time it was
           // noticed the packet was missing.
    conn->audio_buffer[i].sequence_number = 0;
  }
  conn->ab_synced = 0;
  conn->last_seqno_read = -1;
  conn->ab_buffering = 1;
}

void *player_thread_func(void *arg) {
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;

  uint64_t previous_frames_played;
  uint64_t previous_raw_measurement_time;
  uint64_t previous_corrected_measurement_time;
  int previous_frames_played_valid = 0;

  // pthread_cleanup_push(player_thread_initial_cleanup_handler, arg);
  conn->latency_warning_issued =
      0; // be permitted to generate a warning each time a play is attempted
  conn->packet_count = 0;
  conn->packet_count_since_flush = 0;
  conn->previous_random_number = 0;
  conn->decoder_in_use = 0;
  conn->ab_buffering = 1;
  conn->ab_synced = 0;
  conn->first_packet_timestamp = 0;
  conn->flush_requested = 0;
  conn->flush_output_flushed = 0; // only send a flush command to the output device once
  conn->flush_rtp_timestamp = 0;  // it seems this number has a special significance -- it seems to
                                  // be used as a null operand, so we'll use it like that too
  conn->fix_volume = 0x10000;

  conn->ap2_flush_requested = 0;
  conn->ap2_flush_from_valid = 0;
  conn->ap2_rate = 0;
  conn->ap2_play_enabled = 0;

  // reset_anchor_info(conn);

  if (conn->stream.type == ast_apple_lossless)
    init_alac_decoder((int32_t *)&conn->stream.fmtp,
                      conn); // this sets up incoming rate, bit depth, channels.
                             // No pthread cancellation point in here
  // This must be after init_alac_decoder
  init_buffer(conn); // will need a corresponding deallocation. No cancellation
                     // points in here
  ab_resync(conn);

  if (conn->stream.encrypted) {
    AES_set_decrypt_key(conn->stream.aeskey, 128, &conn->aes);
  }

  conn->timestamp_epoch = 0; // indicate that the next timestamp will be the first one.
  conn->maximum_timestamp_interval =
      conn->input_rate * 60; // actually there shouldn't be more than
                             // about 13 seconds of a gap between
                             // successive rtptimes, at worst

  conn->output_sample_ratio = config.output_rate / conn->input_rate;

  // Sign extending rtptime calculations to 64 bit is needed from time to time.

  // The standard rtptime is unsigned 32 bits,
  // so you can do modulo 2^32 difference calculations
  // and get a signed result simply by typing the result as a signed 32-bit
  // number.

  // So long as you can be sure the numbers are within 2^31 of each other,
  // the sign of the result calculated in this way indicates the order of the
  // operands. For example, if you subtract a from b and the result is positive,
  // you can conclude b is the same as or comes after a in module 2^32 order.

  // We want to do the same with the rtptime calculations for multiples of
  // the rtptimes (1, 2, 4 or 8 times), and we want to do this in signed 64-bit/
  // Therefore we need to sign extend these modulo 2^32, 2^33, 2^34, or 2^35 bit
  // unsigned numbers on the same basis.

  // That is what the output_rtptime_sign_bit, output_rtptime_mask,
  // output_rtptime_mask_not and output_rtptime_sign_mask are for -- see later,
  // calculating the sync error.

  int output_rtptime_sign_bit;
  switch (conn->output_sample_ratio) {
    case 1:
      output_rtptime_sign_bit = 31;
      break;
    case 2:
      output_rtptime_sign_bit = 32;
      break;
    case 4:
      output_rtptime_sign_bit = 33;
      break;
    case 8:
      output_rtptime_sign_bit = 34;
      break;
    default:
      debug(1, "error with output ratio -- can't calculate sign bit number");
      output_rtptime_sign_bit = 31;
      break;
  }

  //  debug(1, "Output sample ratio is %d.", conn->output_sample_ratio);
  //  debug(1, "Output output_rtptime_sign_bit: %d.", output_rtptime_sign_bit);

  int64_t output_rtptime_mask = 1;
  output_rtptime_mask = output_rtptime_mask << (output_rtptime_sign_bit + 1);
  output_rtptime_mask = output_rtptime_mask - 1;

  int64_t output_rtptime_mask_not = output_rtptime_mask;
  output_rtptime_mask_not = ~output_rtptime_mask;

  int64_t output_rtptime_sign_mask = 1;
  output_rtptime_sign_mask = output_rtptime_sign_mask << output_rtptime_sign_bit;

  conn->max_frame_size_change =
      1 * conn->output_sample_ratio; // we add or subtract one frame at the
                                     // nominal rate, multiply it by the frame
                                     // ratio. but, on some occasions, more than
                                     // one frame could be added

  switch (config.output_format) {
    case SPS_FORMAT_S24_3LE:
    case SPS_FORMAT_S24_3BE:
      conn->output_bytes_per_frame = 6;
      break;

    case SPS_FORMAT_S24:
    case SPS_FORMAT_S24_LE:
    case SPS_FORMAT_S24_BE:
      conn->output_bytes_per_frame = 8;
      break;
    case SPS_FORMAT_S32:
    case SPS_FORMAT_S32_LE:
    case SPS_FORMAT_S32_BE:
      conn->output_bytes_per_frame = 8;
      break;
    default:
      conn->output_bytes_per_frame = 4;
  }

  debug(3, "Output frame bytes is %d.", conn->output_bytes_per_frame);

  conn->dac_buffer_queue_minimum_length =
      (uint64_t)(config.audio_backend_buffer_interpolation_threshold_in_seconds *
                 config.output_rate);
  debug(3, "dac_buffer_queue_minimum_length is %" PRIu64 " frames.",
        conn->dac_buffer_queue_minimum_length);

  conn->session_corrections = 0;
  conn->connection_state_to_output = get_requested_connection_state_to_output();
// this is about half a minute
//#define trend_interval 3758

// this is about 8 seconds
#define trend_interval 1003

  int number_of_statistics, oldest_statistic, newest_statistic;
  int at_least_one_frame_seen = 0;
  int at_least_one_frame_seen_this_session = 0;
  int64_t tsum_of_sync_errors, tsum_of_corrections, tsum_of_insertions_and_deletions,
      tsum_of_drifts;
  int64_t previous_sync_error = 0, previous_correction = 0;
  uint64_t minimum_dac_queue_size = UINT64_MAX;
  int32_t minimum_buffer_occupancy = INT32_MAX;
  int32_t maximum_buffer_occupancy = INT32_MIN;

  conn->ap2_audio_buffer_minimum_size = -1;

  conn->playstart = time(NULL);

  conn->raw_frame_rate = 0.0;
  conn->corrected_frame_rate = 0.0;
  conn->frame_rate_valid = 0;

  conn->input_frame_rate = 0.0;
  conn->input_frame_rate_starting_point_is_valid = 0;

  conn->buffer_occupancy = 0;

  int play_samples = 0;
  uint64_t current_delay;
  int play_number = 0;
  conn->play_number_after_flush = 0;
  //  int last_timestamp = 0; // for debugging only
  conn->time_of_last_audio_packet = 0;
  // conn->shutdown_requested = 0;
  number_of_statistics = oldest_statistic = newest_statistic = 0;
  tsum_of_sync_errors = tsum_of_corrections = tsum_of_insertions_and_deletions = tsum_of_drifts =
      0;

  const int print_interval = trend_interval; // don't ask...
  // I think it's useful to keep this prime to prevent it from falling into a
  // pattern with some other process.

  static char rnstate[256];
  initstate(time(NULL), rnstate, 256);

  signed short *inbuf;
  int inbuflength;

  unsigned int output_bit_depth = 16; // default;

  switch (config.output_format) {
    case SPS_FORMAT_S8:
    case SPS_FORMAT_U8:
      output_bit_depth = 8;
      break;
    case SPS_FORMAT_S16:
    case SPS_FORMAT_S16_LE:
    case SPS_FORMAT_S16_BE:
      output_bit_depth = 16;
      break;
    case SPS_FORMAT_S24:
    case SPS_FORMAT_S24_LE:
    case SPS_FORMAT_S24_BE:
    case SPS_FORMAT_S24_3LE:
    case SPS_FORMAT_S24_3BE:
      output_bit_depth = 24;
      break;
    case SPS_FORMAT_S32:
    case SPS_FORMAT_S32_LE:
    case SPS_FORMAT_S32_BE:
      output_bit_depth = 32;
      break;
    case SPS_FORMAT_UNKNOWN:
      die("Unknown format choosing output bit depth");
      break;
    case SPS_FORMAT_AUTO:
      die("Invalid format -- SPS_FORMAT_AUTO -- choosing output bit depth");
      break;
    case SPS_FORMAT_INVALID:
      die("Invalid format -- SPS_FORMAT_INVALID -- choosing output bit depth");
      break;
  }

  debug(3, "Output bit depth is %d.", output_bit_depth);

  if (conn->input_bit_depth > output_bit_depth) {
    debug(3, "Dithering will be enabled because the input bit depth is greater "
             "than the output bit "
             "depth");
  }
  if (config.output->parameters == NULL) {
    debug(3, "Dithering will be enabled because the output volume is being "
             "altered in software");
  }

  if ((config.output->parameters == NULL) || (conn->input_bit_depth > output_bit_depth) ||
      (config.playback_mode == ST_mono))
    conn->enable_dither = 1;

  // remember, the output device may never have been initialised prior to this
  // call
  config.output->start(config.output_rate,
                       config.output_format); // will need a corresponding stop

  // we need an intermediate "transition" buffer

  conn->tbuf = malloc(
      sizeof(int32_t) * 2 *
      (conn->max_frames_per_packet * conn->output_sample_ratio + conn->max_frame_size_change));
  if (conn->tbuf == NULL)
    die("Failed to allocate memory for the transition buffer.");

  // initialise this, because soxr stuffing might be chosen later

  conn->sbuf = malloc(
      sizeof(int32_t) * 2 *
      (conn->max_frames_per_packet * conn->output_sample_ratio + conn->max_frame_size_change));
  if (conn->sbuf == NULL)
    die("Failed to allocate memory for the sbuf buffer.");

  // The size of these dependents on the number of frames, the size of each
  // frame and the maximum size change
  conn->outbuf = malloc(
      conn->output_bytes_per_frame *
      (conn->max_frames_per_packet * conn->output_sample_ratio + conn->max_frame_size_change));
  if (conn->outbuf == NULL)
    die("Failed to allocate memory for an output buffer.");
  conn->first_packet_timestamp = 0;
  conn->missing_packets = conn->late_packets = conn->too_late_packets = conn->resend_requests = 0;
  int sync_error_out_of_bounds =
      0; // number of times in a row that there's been a serious sync error

  conn->statistics = malloc(sizeof(stats_t) * trend_interval);
  if (conn->statistics == NULL)
    die("Failed to allocate a statistics buffer");

  conn->framesProcessedInThisEpoch = 0;
  conn->framesGeneratedInThisEpoch = 0;
  conn->correctionsRequestedInThisEpoch = 0;
  statistics_row = 0; // statistics_line 0 means print the headings; anything else 1 means
                      // print the values. Set to 0 the first time out.

  // decide on what statistics profile to use, if requested

  pthread_cleanup_push(player_thread_cleanup_handler,
                       arg); // undo what's been done so far

  // stop looking elsewhere for DACP stuff
  int oldState;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);

  mdns_dacp_monitor_set_id(conn->dacp_id);

  pthread_setcancelstate(oldState, NULL);

  double initial_volume = config.airplay_volume; // default
  if (conn->initial_airplay_volume_set)          // if we have been given an initial
                                                 // volume
    initial_volume = conn->initial_airplay_volume;
  // set the default volume to whatever it was before, as stored in the config
  // airplay_volume
  debug(2, "Set initial volume to %f.", initial_volume);
  player_volume(initial_volume,
                conn); // will contain a cancellation point if asked to wait

  debug(2, "Play begin");
  while (1) {
    // check a few parameters to ensure they are non-zero
    if (config.output_rate == 0)
      debug(1, "config.output_rate is zero!");
    if (conn->output_sample_ratio == 0)
      debug(1, "conn->output_sample_ratio is zero!");
    if (conn->input_rate == 0)
      debug(1, "conn->input_rate is zero!");
    if (conn->input_bytes_per_frame == 0)
      debug(1, "conn->input_bytes_per_frame is zero!");

    pthread_testcancel();                     // allow a pthread_cancel request to take effect.
    abuf_t *inframe = buffer_get_frame(conn); // this has cancellation point(s), but it's not
                                              // guaranteed that they'll always be executed
    uint64_t local_time_now = get_absolute_time_in_ns(); // types okay
    if (inframe) {
      inbuf = inframe->data;
      inbuflength = inframe->length;
      if (inbuf) {
        play_number++;
        //        if (play_number % 100 == 0)
        //          debug(3, "Play frame %d.", play_number);
        conn->play_number_after_flush++;
        if (inframe->given_timestamp == 0) {
          debug(2,
                "Player has supplied a silent frame, (possibly frame %u) for "
                "play number %d, "
                "status 0x%X after %u resend requests.",
                conn->last_seqno_read + 1, play_number, inframe->status,
                inframe->resend_request_number);
          conn->last_seqno_read =
              ((conn->last_seqno_read + 1) & 0xffff); // manage the packet out of sequence minder

          void *silence = malloc(conn->output_bytes_per_frame * conn->max_frames_per_packet *
                                 conn->output_sample_ratio);
          if (silence == NULL) {
            debug(1, "Failed to allocate memory for a silent frame silence buffer.");
          } else {
            // the player may change the contents of the buffer, so it has to be
            // zeroed each time; might as well malloc and free it locally
            conn->previous_random_number = generate_zero_frames(
                silence, conn->max_frames_per_packet * conn->output_sample_ratio,
                config.output_format, conn->enable_dither, conn->previous_random_number);
            config.output->play(silence, conn->max_frames_per_packet * conn->output_sample_ratio);
            free(silence);
          }
        } else if (conn->play_number_after_flush < 10) {
          /*
          int64_t difference = 0;
          if (last_timestamp)
            difference = inframe->timestamp - last_timestamp;
          last_timestamp = inframe->timestamp;
          debug(1, "Play number %d, monotonic timestamp %llx, difference
          %lld.",conn->play_number_after_flush,inframe->timestamp,difference);
          */
          void *silence = malloc(conn->output_bytes_per_frame * conn->max_frames_per_packet *
                                 conn->output_sample_ratio);
          if (silence == NULL) {
            debug(1, "Failed to allocate memory for a flush silence buffer.");
          } else {
            // the player may change the contents of the buffer, so it has to be
            // zeroed each time; might as well malloc and free it locally
            conn->previous_random_number = generate_zero_frames(
                silence, conn->max_frames_per_packet * conn->output_sample_ratio,
                config.output_format, conn->enable_dither, conn->previous_random_number);
            config.output->play(silence, conn->max_frames_per_packet * conn->output_sample_ratio);
            free(silence);
          }
        } else {
          if (((config.output->parameters == NULL) && (config.ignore_volume_control == 0) &&
               (config.airplay_volume != 0.0)) ||
              (conn->input_bit_depth > output_bit_depth) || (config.playback_mode == ST_mono))
            conn->enable_dither = 1;
          else
            conn->enable_dither = 0;

          // here, let's transform the frame of data, if necessary

          switch (conn->input_bit_depth) {
            case 16: {
              int i, j;
              int16_t ls, rs;
              int32_t ll = 0, rl = 0;
              int16_t *inps = inbuf;
              // int16_t *outps = tbuf;
              int32_t *outpl = (int32_t *)conn->tbuf;
              for (i = 0; i < inbuflength; i++) {
                ls = *inps++;
                rs = *inps++;

                // here, do the mode stuff -- mono / reverse stereo / leftonly /
                // rightonly also, raise the 16-bit samples to 32 bits.

                switch (config.playback_mode) {
                  case ST_mono: {
                    int32_t both = ls + rs;
                    both = both << (16 - 1); // keep all 17 bits of the sum of the 16bit left
                                             // and right
                                             // -- the 17th bit will influence dithering later
                    ll = both;
                    rl = both;
                  } break;
                  case ST_reverse_stereo: {
                    ll = rs;
                    rl = ls;
                    ll = ll << 16;
                    rl = rl << 16;
                  } break;
                  case ST_left_only:
                    rl = ls;
                    ll = ls;
                    ll = ll << 16;
                    rl = rl << 16;
                    break;
                  case ST_right_only:
                    ll = rs;
                    rl = rs;
                    ll = ll << 16;
                    rl = rl << 16;
                    break;
                  case ST_stereo:
                    ll = ls;
                    rl = rs;
                    ll = ll << 16;
                    rl = rl << 16;
                    break; // nothing extra to do
                }

                // here, replicate the samples if you're upsampling

                for (j = 0; j < conn->output_sample_ratio; j++) {
                  *outpl++ = ll;
                  *outpl++ = rl;
                }
              }
            } break;
            case 32: {
              int i, j;
              int32_t ls, rs;
              int32_t ll = 0, rl = 0;
              int32_t *inps = (int32_t *)inbuf;
              int32_t *outpl = (int32_t *)conn->tbuf;
              for (i = 0; i < inbuflength; i++) {
                ls = *inps++;
                rs = *inps++;

                // here, do the mode stuff -- mono / reverse stereo / leftonly /
                // rightonly

                switch (config.playback_mode) {
                  case ST_mono: {
                    int64_t both = ls + rs;
                    both = both >> 1;
                    uint32_t both32 = both;
                    ll = both32;
                    rl = both32;
                  } break;
                  case ST_reverse_stereo: {
                    ll = rs;
                    rl = ls;
                  } break;
                  case ST_left_only:
                    rl = ls;
                    ll = ls;
                    break;
                  case ST_right_only:
                    ll = rs;
                    rl = rs;
                    break;
                  case ST_stereo:
                    ll = ls;
                    rl = rs;
                    break; // nothing extra to do
                }

                // here, replicate the samples if you're upsampling

                for (j = 0; j < conn->output_sample_ratio; j++) {
                  *outpl++ = ll;
                  *outpl++ = rl;
                }
              }
            } break;

            default:
              die("Shairport Sync only supports 16 or 32 bit input");
          }

          inbuflength *= conn->output_sample_ratio;

          // We have a frame of data. We need to see if we want to add or remove
          // a frame from it to keep in sync. So we calculate the timing error
          // for the first frame in the DAC. If it's ahead of time, we add one
          // audio frame to this frame to delay a subsequent frame If it's late,
          // we remove an audio frame from this frame to bring a subsequent
          // frame forward in time

          // now, go back as far as the total latency less, say, 100 ms, and
          // check the presence of frames from then onwards

          at_least_one_frame_seen = 1;

          // This is the timing error for the next audio frame in the DAC, if
          // applicable
          int64_t sync_error = 0;

          int amount_to_stuff = 0;

          // check sequencing
          if (conn->last_seqno_read == -1)
            conn->last_seqno_read = inframe->sequence_number; // int32_t from seq_t, i.e. uint16_t,
                                                              // so okay.
          else {
            conn->last_seqno_read = (conn->last_seqno_read + 1) &
                                    0xffff; // int32_t from seq_t, i.e. uint16_t, so okay.
            if (inframe->sequence_number != conn->last_seqno_read) { // seq_t, ei.e. uint16_t and
                                                                     // int32_t, so okay
              debug(2,
                    "Player: packets out of sequence: expected: %u, got: %u, "
                    "with ab_read: %u "
                    "and ab_write: %u.",
                    conn->last_seqno_read, inframe->sequence_number, conn->ab_read,
                    conn->ab_write);
              conn->last_seqno_read = inframe->sequence_number; // reset warning...
            }
          }

          int16_t bo = conn->ab_write - conn->ab_read; // do this in 16 bits
          conn->buffer_occupancy = bo;                 // 32 bits

          if (conn->buffer_occupancy < minimum_buffer_occupancy)
            minimum_buffer_occupancy = conn->buffer_occupancy;

          if (conn->buffer_occupancy > maximum_buffer_occupancy)
            maximum_buffer_occupancy = conn->buffer_occupancy;

          // now, before outputting anything to the output device, check the
          // stats

          if (play_number % print_interval == 0) {
            // here, calculate the input and output frame rates, where possible,
            // even if statistics have not been requested this is to calculate
            // them in case they are needed by the D-Bus interface or elsewhere.

            if (conn->input_frame_rate_starting_point_is_valid) {
              uint64_t elapsed_reception_time, frames_received;
              elapsed_reception_time = conn->frames_inward_measurement_time -
                                       conn->frames_inward_measurement_start_time;
              frames_received = conn->frames_inward_frames_received_at_measurement_time -
                                conn->frames_inward_frames_received_at_measurement_start_time;
              conn->input_frame_rate =
                  (1.0E9 * frames_received) / elapsed_reception_time; // an IEEE double calculation
                                                                      // with two 64-bit integers
            } else {
              conn->input_frame_rate = 0.0;
            }

            int stats_status = 0;
            if ((config.output->delay) && (config.output->stats)) {
              uint64_t frames_sent_for_play;
              uint64_t raw_measurement_time;
              uint64_t corrected_measurement_time;
              uint64_t actual_delay;
              stats_status =
                  config.output->stats(&raw_measurement_time, &corrected_measurement_time,
                                       &actual_delay, &frames_sent_for_play);
              // debug(1,"status: %d, actual_delay: %" PRIu64 ",
              // frames_sent_for_play: %" PRIu64 ", frames_played: %" PRIu64
              // ".", stats_status, actual_delay, frames_sent_for_play,
              // frames_sent_for_play - actual_delay);
              uint64_t frames_played = frames_sent_for_play - actual_delay;
              // If the status is zero, it means that there were no output
              // problems since the last time the stats call was made. Thus, the
              // frame rate should be valid.
              if ((stats_status == 0) && (previous_frames_played_valid != 0)) {
                uint64_t frames_played_in_this_interval = frames_played - previous_frames_played;
                int64_t raw_interval = raw_measurement_time - previous_raw_measurement_time;
                int64_t corrected_interval =
                    corrected_measurement_time - previous_corrected_measurement_time;
                if (raw_interval != 0) {
                  conn->raw_frame_rate = (1e9 * frames_played_in_this_interval) / raw_interval;
                  conn->corrected_frame_rate =
                      (1e9 * frames_played_in_this_interval) / corrected_interval;
                  conn->frame_rate_valid = 1;
                  // debug(1,"frames_played_in_this_interval: %" PRIu64 ",
                  // interval: %" PRId64 ", rate: %f.",
                  //  frames_played_in_this_interval, interval,
                  //  conn->frame_rate);
                }
              }

              // uncomment the if statement if your want to get as long a period
              // for calculating the frame rate as possible without an output
              // break or error
              if ((stats_status != 0) || (previous_frames_played_valid == 0)) {
                // if we have just detected an outputting error, or if we have
                // no starting information
                if (stats_status != 0)
                  conn->frame_rate_valid = 0;
                previous_frames_played = frames_played;
                previous_raw_measurement_time = raw_measurement_time;
                previous_corrected_measurement_time = corrected_measurement_time;
                previous_frames_played_valid = 1;
              }
            }

            // we can now calculate running averages for sync error (frames),
            // corrections (ppm), insertions plus deletions (ppm), drift (ppm)
            double moving_average_sync_error = 0.0;
            double moving_average_correction = 0.0;
            double moving_average_insertions_plus_deletions = 0.0;
            if (number_of_statistics == 0) {
              debug(3, "number_of_statistics is zero!");
            } else {
              moving_average_sync_error = (1.0 * tsum_of_sync_errors) / number_of_statistics;
              moving_average_correction = (1.0 * tsum_of_corrections) / number_of_statistics;
              moving_average_insertions_plus_deletions =
                  (1.0 * tsum_of_insertions_and_deletions) / number_of_statistics;
              // double moving_average_drift = (1.0 * tsum_of_drifts) /
              // number_of_statistics;
            }
            // if ((play_number/print_interval)%20==0)
            // figure out which statistics profile to use, depending on the kind
            // of stream

            if (config.statistics_requested) {
              if (at_least_one_frame_seen) {
                do {
                  line_of_stats[0] = '\0';
                  statistics_column = 0;
                  was_a_previous_column = 0;
                  statistics_item("sync error ms", "%*.2f", 13,
                                  1000 * moving_average_sync_error / config.output_rate);
                  statistics_item("net sync ppm", "%*.1f", 12,
                                  moving_average_correction * 1000000 /
                                      (352 * conn->output_sample_ratio));
                  statistics_item("all sync ppm", "%*.1f", 12,
                                  moving_average_insertions_plus_deletions * 1000000 /
                                      (352 * conn->output_sample_ratio));
                  statistics_item("    packets", "%*d", 11, play_number);
                  statistics_item("missing", "%*" PRIu64 "", 7, conn->missing_packets);
                  statistics_item("  late", "%*" PRIu64 "", 6, conn->late_packets);
                  statistics_item("too late", "%*" PRIu64 "", 8, conn->too_late_packets);
                  statistics_item("resend reqs", "%*" PRIu64 "", 11, conn->resend_requests);
                  statistics_item("min DAC queue", "%*" PRIu64 "", 13, minimum_dac_queue_size);
                  statistics_item("min buffers", "%*" PRIu32 "", 11, minimum_buffer_occupancy);
                  statistics_item("max buffers", "%*" PRIu32 "", 11, maximum_buffer_occupancy);

                  if (conn->ap2_audio_buffer_minimum_size > 10 * 1024)
                    statistics_item("min buffer size", "%*" PRIu32 "k", 14,
                                    conn->ap2_audio_buffer_minimum_size / 1024);
                  else
                    statistics_item("min buffer size", "%*" PRIu32 "", 15,
                                    conn->ap2_audio_buffer_minimum_size);

                  statistics_item("nominal fps", "%*.2f", 11, conn->remote_frame_rate);
                  statistics_item("received fps", "%*.2f", 12, conn->input_frame_rate);
                  if (conn->frame_rate_valid) {
                    statistics_item("output fps (r)", "%*.2f", 14, conn->raw_frame_rate);
                    statistics_item("output fps (c)", "%*.2f", 14, conn->corrected_frame_rate);
                  } else {
                    statistics_item("output fps (r)", "           N/A");
                    statistics_item("output fps (c)", "           N/A");
                  }
                  statistics_item("source drift ppm", "%*.2f", 16,
                                  (conn->local_to_remote_time_gradient - 1.0) * 1000000);
                  statistics_item("drift samples", "%*d", 13,
                                  conn->local_to_remote_time_gradient_sample_count);
                  /*
                  statistics_item("estimated (unused) correction ppm", "%*.2f",
                                  strlen("estimated (unused) correction ppm"),
                                  (conn->frame_rate_valid != 0)
                                      ? ((conn->frame_rate -
                                          conn->remote_frame_rate *
                  conn->output_sample_ratio *
                                              conn->local_to_remote_time_gradient)
                  * 1000000) / conn->frame_rate : 0.0);
                  */
                  statistics_row++;
                  inform(line_of_stats);
                } while (statistics_row < 2);
              } else {
                inform("No frames received in the last sampling interval.");
              }
            }
            minimum_dac_queue_size = UINT64_MAX;  // hack reset
            maximum_buffer_occupancy = INT32_MIN; // can't be less than this
            minimum_buffer_occupancy = INT32_MAX; // can't be more than this

            conn->ap2_audio_buffer_minimum_size = -1;

            at_least_one_frame_seen = 0;
          }

          // here, we want to check (a) if we are meant to do synchronisation,
          // (b) if we have a delay procedure, (c) if we can get the delay.

          // If any of these are false, we don't do any synchronisation stuff

          int resp = -1;      // use this as a flag -- if negative, we can't rely on
                              // a real known delay
          current_delay = -1; // use this as a failure flag

          if (config.output->delay) {
            long l_delay;
            resp = config.output->delay(&l_delay);
            if (resp == 0) { // no error
              current_delay = l_delay;
              if (l_delay >= 0)
                current_delay = l_delay;
              else {
                debug(2, "Underrun of %ld frames reported, but ignored.", l_delay);
                current_delay = 0; // could get a negative value if there was
                                   // underrun, but ignore it.
              }
              if (current_delay < minimum_dac_queue_size) {
                minimum_dac_queue_size = current_delay; // update for display later
              }
            } else {
              current_delay = 0;
              if ((resp == sps_extra_code_output_stalled) &&
                  (conn->unfixable_error_reported == 0)) {
                conn->unfixable_error_reported = 1;

                warn("Connection %d: An unfixable error has been detected -- "
                     "output device is "
                     "stalled. \"No "
                     "run_this_if_an_unfixable_error_is_detected\" command "
                     "provided -- nothing "
                     "done.",
                     conn->connection_number);

              } else
                debug(1, "Delay error %d when checking running latency.", resp);
            }
          }

          if (resp == 0) {
            uint32_t should_be_frame_32;
            // this is denominated in the frame rate of the incoming stream
            local_time_to_frame(local_time_now, &should_be_frame_32, conn);

            int64_t should_be_frame = should_be_frame_32;
            should_be_frame = should_be_frame * conn->output_sample_ratio;

            // current_delay is denominated in the frame rate of the outgoing
            // stream
            int64_t will_be_frame = inframe->given_timestamp;
            will_be_frame = will_be_frame * conn->output_sample_ratio;
            will_be_frame = (will_be_frame - current_delay) &
                            output_rtptime_mask; // this is to make sure it's unsigned
                                                 // modulo 2^bits for the rtptime

            // Now we have a tricky piece of calculation to perform.
            // We know the rtptimes are unsigned in 32 or more bits -- call it r
            // bits. We have to calculate the difference between them. on the
            // basis that they should be within 2^(r-1) of one another, so that
            // the unsigned subtraction result, modulo 2^r, if interpreted as a
            // signed number, should yield the difference _and_ the ordering.

            sync_error = should_be_frame - will_be_frame; // this is done in int64_t form

            // sign-extend the r-bit unsigned int calculation by treating it as
            // an r-bit signed integer
            if ((sync_error & output_rtptime_sign_mask) != 0) { // check what would be the sign bit
                                                                // in "r" bit unsigned arithmetic
              // result is negative
              sync_error = sync_error | output_rtptime_mask_not;
            } else {
              // result is positive
              sync_error = sync_error & output_rtptime_mask;
            }

            if (at_least_one_frame_seen_this_session == 0) {
              at_least_one_frame_seen_this_session = 1;

              // debug(2,"first frame real sync error (positive --> late): %"
              // PRId64 " frames.", sync_error);

              // this is a sneaky attempt to make a final adjustment to the
              // timing of the first packet

              // the very first packet generally has a first_frame_early_bias
              // subtracted from its timing to make it more likely that it will
              // be early than late, making it possible to compensate for it be
              // adding a few frames of silence.

              // debug(2,"first frame real sync error (positive --> late): %"
              // PRId64 " frames.", sync_error);

              // remove the bias when reporting the error to make it the true
              // error

              debug(2,
                    "first frame sync error (positive --> late): %" PRId64
                    " frames, %.3f mS at %d frames per second output.",
                    sync_error + first_frame_early_bias,
                    (1000.0 * (sync_error + first_frame_early_bias)) / config.output_rate,
                    config.output_rate);

              // if the packet is early, add the frames needed to put it in
              // sync.
              if (sync_error < 0) {
                size_t final_adjustment_length_sized = -sync_error;
                char *final_adjustment_silence =
                    malloc(conn->output_bytes_per_frame * final_adjustment_length_sized);
                if (final_adjustment_silence) {
                  conn->previous_random_number = generate_zero_frames(
                      final_adjustment_silence, final_adjustment_length_sized,
                      config.output_format, conn->enable_dither, conn->previous_random_number);
                  int final_adjustment = -sync_error;
                  final_adjustment = final_adjustment - first_frame_early_bias;
                  debug(2,
                        "final sync adjustment: %" PRId64
                        " silent frames added with a bias of %" PRId64 " frames.",
                        -sync_error, first_frame_early_bias);
                  config.output->play(final_adjustment_silence, final_adjustment_length_sized);
                  free(final_adjustment_silence);
                } else {
                  warn("Failed to allocate memory for a "
                       "final_adjustment_silence buffer of %d "
                       "frames for a "
                       "sync error of %d frames.",
                       final_adjustment_length_sized, sync_error);
                }
                sync_error = 0; // say the error was fixed!
              }
            }
            // not too sure if abs() is implemented for int64_t, so we'll do it
            // manually
            int64_t abs_sync_error = sync_error;
            if (abs_sync_error < 0)
              abs_sync_error = -abs_sync_error;

            if ((inframe->given_timestamp != 0) && (config.resyncthreshold > 0.0) &&
                (abs_sync_error > config.resyncthreshold * config.output_rate)) {
              sync_error_out_of_bounds++;
            } else {
              sync_error_out_of_bounds = 0;
            }

            if (sync_error_out_of_bounds > 3) {
              // debug(1, "lost sync with source for %d consecutive packets --
              // flushing and "
              //          "resyncing. Error: %lld.",
              //        sync_error_out_of_bounds, sync_error);
              sync_error_out_of_bounds = 0;

              uint64_t frames_sent_for_play = 0;
              uint64_t actual_delay = 0;

              if ((config.output->delay) && (config.output->stats)) {
                uint64_t raw_measurement_time;
                uint64_t corrected_measurement_time;
                config.output->stats(&raw_measurement_time, &corrected_measurement_time,
                                     &actual_delay, &frames_sent_for_play);
              }

              int64_t filler_length =
                  (int64_t)(config.resyncthreshold * config.output_rate); // number of samples
              if ((sync_error > 0) && (sync_error > filler_length)) {
                debug(1,
                      "Large positive sync error of %" PRId64
                      " frames (%f seconds), at frame: %" PRIu32 ".",
                      sync_error, (sync_error * 1.0) / config.output_rate,
                      inframe->given_timestamp);
                debug(1, "%" PRId64 " frames sent to DAC. DAC buffer contains %" PRId64 " frames.",
                      frames_sent_for_play, actual_delay);
                // the sync error is output frames, but we have to work out how
                // many source frames to drop there may be a multiple (the
                // conn->output_sample_ratio) of output frames per input
                // frame...
                int64_t source_frames_to_drop = sync_error;
                source_frames_to_drop = source_frames_to_drop / conn->output_sample_ratio;

                // add some time to give the pipeline a chance to recover -- a
                // bit hacky
                double extra_time_to_drop = 0.1; // seconds
                int64_t extra_frames_to_drop = (int64_t)(conn->input_rate * extra_time_to_drop);
                source_frames_to_drop += extra_frames_to_drop;

                uint32_t frames_to_drop = source_frames_to_drop;
                uint32_t flush_to_frame = inframe->given_timestamp + frames_to_drop;

                do_flush(flush_to_frame, conn);
              } else if ((sync_error < 0) && ((-sync_error) > filler_length)) {
                debug(1,
                      "Large negative sync error of %" PRId64
                      " frames (%f seconds), at frame: %" PRIu32 ".",
                      sync_error, (sync_error * 1.0) / config.output_rate,
                      inframe->given_timestamp);
                debug(1, "%" PRId64 " frames sent to DAC. DAC buffer contains %" PRId64 " frames.",
                      frames_sent_for_play, actual_delay);
                int64_t silence_length = -sync_error;
                if (silence_length > (filler_length * 5))
                  silence_length = filler_length * 5;
                size_t silence_length_sized = silence_length;
                char *long_silence = malloc(conn->output_bytes_per_frame * silence_length_sized);
                if (long_silence) {
                  conn->previous_random_number = generate_zero_frames(
                      long_silence, silence_length_sized, config.output_format,
                      conn->enable_dither, conn->previous_random_number);

                  debug(2, "Play a silence of %d frames.", silence_length_sized);
                  config.output->play(long_silence, silence_length_sized);
                  free(long_silence);
                } else {
                  warn("Failed to allocate memory for a long_silence buffer of "
                       "%d frames for a "
                       "sync error of %d frames.",
                       silence_length_sized, sync_error);
                }
                reset_input_flow_metrics(conn);
              }
            } else {
              /*
              // before we finally commit to this frame, check its sequencing
              and timing
              // require a certain error before bothering to fix it...
              if (sync_error > config.tolerance * config.output_rate) { //
              int64_t > int, okay amount_to_stuff = -1;
              }
              if (sync_error < -config.tolerance * config.output_rate) {
                amount_to_stuff = 1;
              }
              */

              if (amount_to_stuff == 0) {
                // use a "V" shaped function to decide if stuffing should occur
                int64_t s = r64i();
                s = s >> 31;
                s = s * config.tolerance * config.output_rate;
                s = (s >> 32) + config.tolerance * config.output_rate; // should be a number from 0
                                                                       // to config.tolerance *
                                                                       // config.output_rate;
                if ((sync_error > 0) && (sync_error > s)) {
                  // debug(1,"Extra stuff -1");
                  amount_to_stuff = -1;
                }
                if ((sync_error < 0) && (sync_error < (-s))) {
                  // debug(1,"Extra stuff +1");
                  amount_to_stuff = 1;
                }
              }

              // try to keep the corrections definitely below 1 in 1000 audio
              // frames

              // calculate the time elapsed since the play session started.

              if (amount_to_stuff) {
                if ((local_time_now) && (conn->first_packet_time_to_play) &&
                    (local_time_now >= conn->first_packet_time_to_play)) {
                  int64_t tp = (local_time_now - conn->first_packet_time_to_play) /
                               1000000000; // seconds int64_t from uint64_t which is
                                           // always positive, so ok

                  if (tp < 5)
                    amount_to_stuff = 0; // wait at least five seconds
                  /*
                  else if (tp < 30) {
                    if ((random() % 1000) >
                        352) // keep it to about 1:1000 for the first thirty
                  seconds amount_to_stuff = 0;
                  }
                  */
                }
              }

              // Apply DSP here

              // check the state of loudness and convolution flags here and
              // don't change them for the frame

              int do_loudness = config.loudness;

              if (do_loudness) {
                int32_t *tbuf32 = (int32_t *)conn->tbuf;
                float fbuf_l[inbuflength];
                float fbuf_r[inbuflength];

                // Deinterleave, and convert to float
                int i;
                for (i = 0; i < inbuflength; ++i) {
                  fbuf_l[i] = tbuf32[2 * i];
                  fbuf_r[i] = tbuf32[2 * i + 1];
                }

                if (do_loudness) {
                  // Apply volume and loudness
                  // Volume must be applied here because the loudness filter
                  // will increase the signal level and it would saturate the
                  // int32_t otherwise
                  float gain = conn->fix_volume / 65536.0f;
                  // float gain_db = 20 * log10(gain);
                  // debug(1, "Applying soft volume dB: %f k: %f", gain_db,
                  // gain);

                  for (i = 0; i < inbuflength; ++i) {
                    fbuf_l[i] = loudness_process(&loudness_l, fbuf_l[i] * gain);
                    fbuf_r[i] = loudness_process(&loudness_r, fbuf_r[i] * gain);
                  }
                }

                // Interleave and convert back to int32_t
                for (i = 0; i < inbuflength; ++i) {
                  tbuf32[2 * i] = fbuf_l[i];
                  tbuf32[2 * i + 1] = fbuf_r[i];
                }
              }

              play_samples =
                  stuff_buffer_basic_32((int32_t *)conn->tbuf, inbuflength, config.output_format,
                                        conn->outbuf, amount_to_stuff, conn->enable_dither, conn);

              /*
              {
                int co;
                int is_silent=1;
                short *p = outbuf;
                for (co=0;co<play_samples;co++) {
                  if (*p!=0)
                    is_silent=0;
                  p++;
                }
                if (is_silent)
                  debug(1,"Silence!");
              }
              */

              if (conn->outbuf == NULL)
                debug(1, "NULL outbuf to play -- skipping it.");
              else {
                if (play_samples == 0)
                  debug(1, "play_samples==0 skipping it (1).");
                else {
                  if (conn->software_mute_enabled) {
                    generate_zero_frames(conn->outbuf, play_samples, config.output_format,
                                         conn->enable_dither, conn->previous_random_number);
                  }
                  config.output->play(conn->outbuf, play_samples);
                }
              }

              // check for loss of sync
              // timestamp of zero means an inserted silent frame in place of a
              // missing frame
              /*
              if ((inframe->timestamp != 0) &&
                  && (config.resyncthreshold > 0.0) &&
                  (abs_sync_error > config.resyncthreshold *
              config.output_rate)) { sync_error_out_of_bounds++;
                // debug(1,"Sync error out of bounds: Error: %lld; previous
              error: %lld; DAC: %lld;
                // timestamp: %llx, time now
                //
              %llx",sync_error,previous_sync_error,current_delay,inframe->timestamp,local_time_now);
                if (sync_error_out_of_bounds > 3) {
                  debug(1, "Lost sync with source for %d consecutive packets --
              flushing and " "resyncing. Error: %lld.",
                        sync_error_out_of_bounds, sync_error);
                  sync_error_out_of_bounds = 0;
                  player_flush(nt, conn);
                }
              } else {
                sync_error_out_of_bounds = 0;
              }
              */
            }
          } else {
            // if this is the first frame, see if it's close to when it's
            // supposed to be release, which will be its time plus latency and
            // any offset_time
            if (at_least_one_frame_seen_this_session == 0) {
              at_least_one_frame_seen_this_session = 1;
            }

            play_samples =
                stuff_buffer_basic_32((int32_t *)conn->tbuf, inbuflength, config.output_format,
                                      conn->outbuf, 0, conn->enable_dither, conn);
            if (conn->outbuf == NULL)
              debug(1, "NULL outbuf to play -- skipping it.");
            else {
              if (conn->software_mute_enabled) {
                generate_zero_frames(conn->outbuf, play_samples, config.output_format,
                                     conn->enable_dither, conn->previous_random_number);
              }
              config.output->play(conn->outbuf,
                                  play_samples); // remove the (short*)!
            }
          }

          // mark the frame as finished
          inframe->given_timestamp = 0;
          inframe->sequence_number = 0;
          inframe->resend_time = 0;
          inframe->initialisation_time = 0;

          // update the watchdog
          if ((config.dont_check_timeout == 0) && (config.timeout != 0)) {
            uint64_t time_now = get_absolute_time_in_ns();
            debug_mutex_lock(&conn->watchdog_mutex, 1000, 0);
            conn->watchdog_bark_time = time_now;
            debug_mutex_unlock(&conn->watchdog_mutex, 0);
          }

          // debug(1,"Sync error %lld frames. Amount to stuff %d."
          // ,sync_error,amount_to_stuff);

          // new stats calculation. We want a running average of sync error,
          // drift, adjustment, number of additions+subtractions

          // this is a misleading hack -- the statistics should include some
          // data on the number of valid samples and the number of times sync
          // wasn't checked due to non availability of a delay figure. for the
          // present, stats are only updated when sync has been checked
          if (config.output->delay != NULL) {
            if (number_of_statistics == trend_interval) {
              // here we remove the oldest statistical data and take it from the
              // summaries as well
              tsum_of_sync_errors -= conn->statistics[oldest_statistic].sync_error;
              tsum_of_drifts -= conn->statistics[oldest_statistic].drift;
              if (conn->statistics[oldest_statistic].correction > 0)
                tsum_of_insertions_and_deletions -= conn->statistics[oldest_statistic].correction;
              else
                tsum_of_insertions_and_deletions += conn->statistics[oldest_statistic].correction;
              tsum_of_corrections -= conn->statistics[oldest_statistic].correction;
              oldest_statistic = (oldest_statistic + 1) % trend_interval;
              number_of_statistics--;
            }

            conn->statistics[newest_statistic].sync_error = sync_error;
            conn->statistics[newest_statistic].correction = conn->amountStuffed;

            if (number_of_statistics == 0)
              conn->statistics[newest_statistic].drift = 0;
            else
              conn->statistics[newest_statistic].drift =
                  sync_error - previous_sync_error - previous_correction;

            previous_sync_error = sync_error;
            previous_correction = conn->amountStuffed;

            tsum_of_sync_errors += sync_error;
            tsum_of_drifts += conn->statistics[newest_statistic].drift;
            if (conn->amountStuffed > 0) {
              tsum_of_insertions_and_deletions += conn->amountStuffed;
            } else {
              tsum_of_insertions_and_deletions -= conn->amountStuffed;
            }
            tsum_of_corrections += conn->amountStuffed;
            conn->session_corrections += conn->amountStuffed;

            newest_statistic = (newest_statistic + 1) % trend_interval;
            number_of_statistics++;
          }
        }
      }
    }
  }

  debug(1, "This should never be called.");
  pthread_cleanup_pop(1); // pop the cleanup handler
                          //  debug(1, "This should never be called either.");
                          //  pthread_cleanup_pop(1); // pop the initial cleanup handler
  pthread_exit(NULL);
}

static int init_alac_decoder(int32_t fmtp[12], rtsp_conn_info *conn) {
  // This is a guess, but the format of the fmtp looks identical to the format
  // of an ALACSpecificCOnfig which is detailed in the file
  // ALACMagicCookieDescription.txt in the Apple ALAC sample implementation Here
  // it is:

  /*
      struct  ALACSpecificConfig (defined in ALACAudioTypes.h)
      abstract     This struct is used to describe codec provided information
     about the encoded Apple Lossless bitstream. It must accompany the encoded
     stream in the containing audio file and be provided to the decoder.

      field        frameLength     uint32_t  indicating the frames per packet
     when
     no
     explicit
     frames per packet setting is
                                                            present in the
     packet header. The encoder frames per packet can be explicitly set but for
     maximum compatibility, the default encoder setting of 4096 should be used.

      field        compatibleVersion   uint8_t   indicating compatible version,
                                                            value must be set to
     0

      field        bitDepth     uint8_t   describes the bit depth of the
     source
     PCM
     data
     (maximum
     value = 32)

      field        pb       uint8_t   currently unused tuning
     parametetbugr.
                                                            value should be set
     to 40

      field        mb       uint8_t   currently unused tuning parameter.
                                                            value should be set
     to 14

      field        kb      uint8_t   currently unused tuning parameter.
                                                            value should be set
     to 10

      field        numChannels     uint8_t   describes the channel count (1 =
     mono,
     2
     =
     stereo,
     etc...)
                                                            when channel layout
     info is not provided in the 'magic cookie', a channel count > 2 describes a
     set of discreet channels with no specific ordering

      field        maxRun      uint16_t   currently unused.
                                                    value should be set to 255

      field        maxFrameBytes     uint32_t   the maximum size of an Apple
     Lossless
     packet
     within
     the encoded stream.
                                                          value of 0 indicates
     unknown

      field        avgBitRate     uint32_t   the average bit rate in bits per
     second
     of
     the
     Apple
     Lossless stream.
                                                          value of 0 indicates
     unknown

      field        sampleRate     uint32_t   sample rate of the encoded stream
   */

  // We are going to go on that basis

  alac_file *alac;

  alac = alac_create(conn->input_bit_depth,
                     conn->input_num_channels); // no pthread cancellation point in here
  if (!alac)
    return 1;
  conn->decoder_info = alac;

  alac->setinfo_max_samples_per_frame = conn->max_frames_per_packet;
  alac->setinfo_7a = fmtp[2];
  alac->setinfo_sample_size = conn->input_bit_depth;
  alac->setinfo_rice_historymult = fmtp[4];
  alac->setinfo_rice_initialhistory = fmtp[5];
  alac->setinfo_rice_kmodifier = fmtp[6];
  alac->setinfo_7f = fmtp[7];
  alac->setinfo_80 = fmtp[8];
  alac->setinfo_82 = fmtp[9];
  alac->setinfo_86 = fmtp[10];
  alac->setinfo_8a_rate = fmtp[11];
  alac_allocate_buffers(alac); // no pthread cancellation point in here

  return 0;
}

void avahi_dacp_monitor_set_id(const char *dacp_id) {
  // debug(1, "avahi_dacp_monitor_set_id: Search for DACP ID \"%s\".", t);
  dacp_browser_struct *dbs = &private_dbs;

  if (((dbs->dacp_id) && (dacp_id) && (strcmp(dbs->dacp_id, dacp_id) == 0)) ||
      ((dbs->dacp_id == NULL) && (dacp_id == NULL))) {
    debug(3, "no change...");
  } else {
    if (dbs->dacp_id)
      free(dbs->dacp_id);
    if (dacp_id == NULL)
      dbs->dacp_id = NULL;
    else {
      char *t = strdup(dacp_id);
      if (t) {
        dbs->dacp_id = t;
        avahi_threaded_poll_lock(tpoll);
        if (dbs->service_browser)
          avahi_service_browser_free(dbs->service_browser);

        if (!(dbs->service_browser = avahi_service_browser_new(
                  client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_dacp._tcp", NULL, 0,
                  browse_callback, (void *)dbs))) {
          warn("failed to create avahi service browser: %s\n",
               avahi_strerror(avahi_client_errno(client)));
        }
        avahi_threaded_poll_unlock(tpoll);
        debug(2, "dacp_monitor for \"%s\"", dacp_id);
      } else {
        warn("avahi_dacp_set_id: can not allocate a dacp_id string in "
             "dacp_browser_struct.");
      }
    }
  }
}

void player_volume_without_notification(double airplay_volume, rtsp_conn_info *conn) {
  debug_mutex_lock(&conn->volume_control_mutex, 5000, 1);
  // first, see if we are hw only, sw only, both with hw attenuation on the top
  // or both with sw attenuation on top

  enum volume_mode_type { vol_sw_only, vol_hw_only, vol_both } volume_mode;

  // take account of whether there is a hardware mixer, if a max volume has been
  // specified and if a range has been specified the range might imply that both
  // hw and software mixers are needed, so calculate this

  int32_t hw_max_db = 0,
          hw_min_db = 0; // zeroed to quieten an incorrect uninitialised warning
  int32_t sw_max_db = 0, sw_min_db = -9630;

  if (config.output->parameters) {
    volume_mode = vol_hw_only;
    audio_parameters audio_information;
    config.output->parameters(&audio_information);
    hw_max_db = audio_information.maximum_volume_dB;
    hw_min_db = audio_information.minimum_volume_dB;
    if (config.volume_max_db_set) {
      if (((config.volume_max_db * 100) <= hw_max_db) &&
          ((config.volume_max_db * 100) >= hw_min_db))
        hw_max_db = (int32_t)config.volume_max_db * 100;
      else if (config.volume_range_db) {
        hw_max_db = hw_min_db;
        sw_max_db = (config.volume_max_db * 100) - hw_min_db;
      } else {
        warn("The maximum output level is outside the range of the hardware "
             "mixer -- ignored");
      }
    }

    // here, we have set limits on the hw_max_db and the sw_max_db
    // but we haven't actually decided whether we need both hw and software
    // attenuation only if a range is specified could we need both
    if (config.volume_range_db) {
      // see if the range requested exceeds the hardware range available
      int32_t desired_range_db = (int32_t)trunc(config.volume_range_db * 100);
      if ((desired_range_db) > (hw_max_db - hw_min_db)) {
        volume_mode = vol_both;
        int32_t desired_sw_range = desired_range_db - (hw_max_db - hw_min_db);
        if ((sw_max_db - desired_sw_range) < sw_min_db)
          warn("The range requested is too large to accommodate -- ignored.");
        else
          sw_min_db = (sw_max_db - desired_sw_range);
      } else {
        hw_min_db = hw_max_db - desired_range_db;
      }
    }
  } else {
    // debug(1,"has no hardware mixer");
    volume_mode = vol_sw_only;
    if (config.volume_max_db_set) {
      if (((config.volume_max_db * 100) <= sw_max_db) &&
          ((config.volume_max_db * 100) >= sw_min_db))
        sw_max_db = (int32_t)config.volume_max_db * 100;
    }
    if (config.volume_range_db) {
      // see if the range requested exceeds the software range available
      int32_t desired_range_db = (int32_t)trunc(config.volume_range_db * 100);
      if ((desired_range_db) > (sw_max_db - sw_min_db))
        warn("The range requested is too large to accommodate -- ignored.");
      else
        sw_min_db = (sw_max_db - desired_range_db);
    }
  }

  // here, we know whether it's hw volume control only, sw only or both, and we
  // have the hw and sw limits. if it's both, we haven't decided whether hw or
  // sw should be on top we have to consider the settings ignore_volume_control
  // and mute.

  if (airplay_volume == -144.0) {
    if ((config.output->mute) && (config.output->mute(1) == 0))
      debug(2,
            "player_volume_without_notification: volume mode is %d, "
            "airplay_volume is %f, "
            "hardware mute is enabled.",
            volume_mode, airplay_volume);
    else {
      conn->software_mute_enabled = 1;
      debug(2,
            "player_volume_without_notification: volume mode is %d, "
            "airplay_volume is %f, "
            "software mute is enabled.",
            volume_mode, airplay_volume);
    }
  } else {
    int32_t max_db = 0, min_db = 0;
    switch (volume_mode) {
      case vol_hw_only:
        max_db = hw_max_db;
        min_db = hw_min_db;
        break;
      case vol_sw_only:
        max_db = sw_max_db;
        min_db = sw_min_db;
        break;
      case vol_both:
        // debug(1, "dB range passed is hw: %d, sw: %d, total: %d", hw_max_db -
        // hw_min_db,
        //      sw_max_db - sw_min_db, (hw_max_db - hw_min_db) + (sw_max_db -
        //      sw_min_db));
        max_db = (hw_max_db - hw_min_db) +
                 (sw_max_db - sw_min_db); // this should be the range requested
        min_db = 0;
        break;
      default:
        debug(1, "player_volume_without_notification: error: not in a volume mode");
        break;
    }
    double scaled_attenuation = max_db;
    if (config.ignore_volume_control == 0) {
      if (config.volume_control_profile == VCP_standard)
        scaled_attenuation = vol2attn(airplay_volume, max_db, min_db); // no cancellation points
      else if (config.volume_control_profile == VCP_flat)
        scaled_attenuation = flat_vol2attn(airplay_volume, max_db,
                                           min_db); // no cancellation points
      else
        debug(1, "player_volume_without_notification: unrecognised volume "
                 "control profile");
    }
    // so here we have the scaled attenuation. If it's for hw or sw only, it's
    // straightforward.
    double hardware_attenuation = 0.0;
    double software_attenuation = 0.0;

    switch (volume_mode) {
      case vol_hw_only:
        hardware_attenuation = scaled_attenuation;
        break;
      case vol_sw_only:
        software_attenuation = scaled_attenuation;
        break;
      case vol_both:
        // here, we now the attenuation required, so we have to apportion it to
        // the sw and hw mixers if we give the hw priority, that means when
        // lowering the volume, set the hw volume to its lowest before using the
        // sw attenuation. similarly, if we give the sw priority, that means when
        // lowering the volume, set the sw volume to its lowest before using the
        // hw attenuation. one imagines that hw priority is likely to be much
        // better if (config.volume_range_hw_priority) {
        if (config.volume_range_hw_priority != 0) {
          // hw priority
          if ((sw_max_db - sw_min_db) > scaled_attenuation) {
            software_attenuation = sw_min_db + scaled_attenuation;
            hardware_attenuation = hw_min_db;
          } else {
            software_attenuation = sw_max_db;
            hardware_attenuation = hw_min_db + scaled_attenuation - (sw_max_db - sw_min_db);
          }
        } else {
          // sw priority
          if ((hw_max_db - hw_min_db) > scaled_attenuation) {
            hardware_attenuation = hw_min_db + scaled_attenuation;
            software_attenuation = sw_min_db;
          } else {
            hardware_attenuation = hw_max_db;
            software_attenuation = sw_min_db + scaled_attenuation - (hw_max_db - hw_min_db);
          }
        }
        break;
      default:
        debug(1, "player_volume_without_notification: error: not in a volume mode");
        break;
    }

    if (((volume_mode == vol_hw_only) || (volume_mode == vol_both)) && (config.output->volume)) {
      config.output->volume(hardware_attenuation); // otherwise set the output to the lowest value
      // debug(1,"Hardware attenuation set to %f for airplay volume of
      // %f.",hardware_attenuation,airplay_volume);
      if (volume_mode == vol_hw_only)
        conn->fix_volume = 0x10000;
    }

    if ((volume_mode == vol_sw_only) || (volume_mode == vol_both)) {
      double temp_fix_volume = 65536.0 * pow(10, software_attenuation / 2000);

      if (config.ignore_volume_control == 0)
        debug(2,
              "Software attenuation set to %f, i.e %f out of 65,536, for "
              "airplay volume of %f",
              software_attenuation, temp_fix_volume, airplay_volume);
      else
        debug(2,
              "Software attenuation set to %f, i.e %f out of 65,536. Volume "
              "control is ignored.",
              software_attenuation, temp_fix_volume);

      conn->fix_volume = temp_fix_volume;

      // if (config.loudness)
      loudness_set_volume(software_attenuation / 100);
    }

    if (config.logOutputLevel) {
      inform("Output Level set to: %.2f dB.", scaled_attenuation / 100.0);
    }

    if (config.output->mute)
      config.output->mute(0);
    conn->software_mute_enabled = 0;

    debug(2,
          "player_volume_without_notification: volume mode is %d, airplay "
          "volume is %f, "
          "software_attenuation: %f, hardware_attenuation: %f, muting "
          "is disabled.",
          volume_mode, airplay_volume, software_attenuation, hardware_attenuation);
  }
  // here, store the volume for possible use in the future
  config.airplay_volume = airplay_volume;
  debug_mutex_unlock(&conn->volume_control_mutex, 3);
}

void player_volume(double airplay_volume, rtsp_conn_info *conn) {
  player_volume_without_notification(airplay_volume, conn);
}

// get the next frame, when available. return 0 if underrun/stream reset.
static abuf_t *buffer_get_frame(rtsp_conn_info *conn) {
  // int16_t buf_fill;
  uint64_t local_time_now;
  // struct timespec tn;
  abuf_t *curframe = NULL;
  int notified_buffer_empty = 0; // diagnostic only

  debug_mutex_lock(&conn->ab_mutex, 30000, 0);

  int wait;
  long dac_delay = 0; // long because alsa returns a long

  int have_sent_prefiller_silence =
      0; // set to true when we have sent at least one silent frame to the DAC

  pthread_cleanup_push(buffer_get_frame_cleanup_handler,
                       (void *)conn); // undo what's been done so far
  do {
    // get the time
    local_time_now = get_absolute_time_in_ns(); // type okay
    // debug(3, "buffer_get_frame is iterating");

    // we must have timing information before we can do anything here
    if (have_timestamp_timing_information(conn)) {
      int rco = get_requested_connection_state_to_output();

      if (conn->connection_state_to_output != rco) {
        conn->connection_state_to_output = rco;
        // change happening
        if (conn->connection_state_to_output == 0) { // going off
          debug(2, "request flush because connection_state_to_output is off");
          debug_mutex_lock(&conn->flush_mutex, 1000, 1);
          conn->flush_requested = 1;
          conn->flush_rtp_timestamp = 0;
          debug_mutex_unlock(&conn->flush_mutex, 3);
        }
      }

      if (config.output->is_running)
        if (config.output->is_running() != 0) { // if the back end isn't running for any reason
          debug(2, "request flush because back end is not running");
          debug_mutex_lock(&conn->flush_mutex, 1000, 0);
          conn->flush_requested = 1;
          conn->flush_rtp_timestamp = 0;
          debug_mutex_unlock(&conn->flush_mutex, 0);
        }
      debug_mutex_lock(&conn->flush_mutex, 1000, 0);
      pthread_cleanup_push(mutex_unlock, &conn->flush_mutex);
      if (conn->flush_requested == 1) {
        if (conn->flush_output_flushed == 0)
          if (config.output->flush) {
            config.output->flush(); // no cancellation points
            debug(2, "flush request: flush output device.");
          }
        conn->flush_output_flushed = 1;
      }
      // now check to see it the flush request is for frames in the buffer or
      // not if the first_packet_timestamp is zero, don't check
      int flush_needed = 0;
      int drop_request = 0;
      if (conn->flush_requested == 1) {
        if (conn->flush_rtp_timestamp == 0) {
          debug(1, "flush request: flush frame 0 -- flush assumed to be needed.");
          flush_needed = 1;
          drop_request = 1;
        } else {
          if ((conn->ab_synced) && ((conn->ab_write - conn->ab_read) > 0)) {
            abuf_t *firstPacket = conn->audio_buffer + BUFIDX(conn->ab_read);
            abuf_t *lastPacket = conn->audio_buffer + BUFIDX(conn->ab_write - 1);
            if ((firstPacket != NULL) && (firstPacket->ready)) {
              // discard flushes more than 10 seconds into the future -- they
              // are probably bogus
              uint32_t first_frame_in_buffer = firstPacket->given_timestamp;
              int32_t offset_from_first_frame =
                  (int32_t)(conn->flush_rtp_timestamp - first_frame_in_buffer);
              if (offset_from_first_frame > (int)conn->input_rate * 10) {
                debug(1,
                      "flush request: sanity check -- flush frame %u is too "
                      "far into the future from "
                      "the first frame %u -- discarded.",
                      conn->flush_rtp_timestamp, first_frame_in_buffer);
                drop_request = 1;
              } else {
                if ((lastPacket != NULL) && (lastPacket->ready)) {
                  // we have enough information to check if the flush is needed
                  // or can be discarded
                  uint32_t last_frame_in_buffer =
                      lastPacket->given_timestamp + lastPacket->length - 1;
                  // now we have to work out if the flush frame is in the buffer
                  // if it is later than the end of the buffer, flush everything
                  // and keep the request active. if it is in the buffer, we
                  // need to flush part of the buffer. Actually we flush the
                  // entire buffer and drop the request. if it is before the
                  // buffer, no flush is needed. Drop the request.
                  if (offset_from_first_frame > 0) {
                    int32_t offset_to_last_frame =
                        (int32_t)(last_frame_in_buffer - conn->flush_rtp_timestamp);
                    if (offset_to_last_frame >= 0) {
                      debug(2,
                            "flush request: flush frame %u active -- buffer "
                            "contains %u frames, from "
                            "%u to %u",
                            conn->flush_rtp_timestamp,
                            last_frame_in_buffer - first_frame_in_buffer + 1,
                            first_frame_in_buffer, last_frame_in_buffer);
                      drop_request = 1;
                      flush_needed = 1;
                    } else {
                      debug(2,
                            "flush request: flush frame %u pending -- buffer "
                            "contains %u frames, "
                            "from "
                            "%u to %u",
                            conn->flush_rtp_timestamp,
                            last_frame_in_buffer - first_frame_in_buffer + 1,
                            first_frame_in_buffer, last_frame_in_buffer);
                      flush_needed = 1;
                    }
                  } else {
                    debug(2,
                          "flush request: flush frame %u expired -- buffer "
                          "contains %u frames, "
                          "from %u "
                          "to %u",
                          conn->flush_rtp_timestamp,
                          last_frame_in_buffer - first_frame_in_buffer + 1, first_frame_in_buffer,
                          last_frame_in_buffer);
                    drop_request = 1;
                  }
                }
              }
            }
          } else {
            debug(3,
                  "flush request: flush frame %u  -- buffer not synced or "
                  "empty: synced: %d, "
                  "ab_read: "
                  "%u, ab_write: %u",
                  conn->flush_rtp_timestamp, conn->ab_synced, conn->ab_read, conn->ab_write);
            conn->flush_requested = 0; // remove the request
            // leave flush request pending and don't do a buffer flush, because
            // there isn't one
          }
        }
      }
      if (flush_needed) {
        debug(2, "flush request: flush done.");
        ab_resync(conn); // no cancellation points
        conn->first_packet_timestamp = 0;
        conn->first_packet_time_to_play = 0;
        conn->time_since_play_started = 0;
        have_sent_prefiller_silence = 0;
        dac_delay = 0;
      }
      if (drop_request) {
        debug(2, "flush request: request dropped.");
        conn->flush_requested = 0;
        conn->flush_rtp_timestamp = 0;
        conn->flush_output_flushed = 0;
      }
      pthread_cleanup_pop(1); // unlock the conn->flush_mutex
      if (conn->ab_synced) {
        curframe = conn->audio_buffer + BUFIDX(conn->ab_read);

        if ((conn->ab_read != conn->ab_write) &&
            (curframe->ready)) { // it could be synced and empty, under
          // exceptional circumstances, with the
          // frame unused, thus apparently ready

          if (curframe->sequence_number != conn->ab_read) {
            // some kind of sync problem has occurred.
            if (BUFIDX(curframe->sequence_number) == BUFIDX(conn->ab_read)) {
              // it looks like aliasing has happened
              // jump to the new incoming stuff...
              conn->ab_read = curframe->sequence_number;
              debug(1, "Aliasing of buffer index -- reset.");
            } else {
              debug(1, "Inconsistent sequence numbers detected");
            }
          }
        }

        if ((curframe) && (curframe->ready)) {
          notified_buffer_empty = 0; // at least one buffer now -- diagnostic only.
          if (conn->ab_buffering) {  // if we are getting packets but not yet
                                     // forwarding them to the
            // player
            if (conn->first_packet_timestamp == 0) { // if this is the very first packet
              // debug(1,"First frame seen with timestamp...");
              conn->first_packet_timestamp =
                  curframe->given_timestamp; // we will keep buffering until we are
                                             // supposed to start playing this

              // Here, calculate when we should start playing. We need to know
              // when to allow the packets to be sent to the player.

              // every second or so, we get a reference on when a particular
              // packet should be played.

              // It probably won't  be the timestamp of our first packet,
              // however, so we might have to do some calculations.

              // To calculate when the first packet will be played, we figure
              // out the exact time the packet should be played according to its
              // timestamp and the reference time. The desired latency,
              // typically 88200 frames, will be calculated for in rtp.c, and
              // any desired backend latency offset included in it there.

              uint64_t should_be_time;

              frame_to_local_time(conn->first_packet_timestamp, // this will go modulo 2^32
                                  &should_be_time, conn);

              conn->first_packet_time_to_play = should_be_time;

              int64_t lt = conn->first_packet_time_to_play - local_time_now;

              if (lt < 100000000) {
                debug(1,
                      "Connection %d: Short lead time for first frame %" PRId64
                      ": %f seconds. Flushing 0.5 seconds",
                      conn->connection_number, conn->first_packet_timestamp, lt * 0.000000001);
                do_flush(conn->first_packet_timestamp + 5 * 4410, conn);
              } else {
                debug(2, "Connection %d: Lead time for first frame %" PRId64 ": %f seconds.",
                      conn->connection_number, conn->first_packet_timestamp, lt * 0.000000001);
              }
              /*
                            int64_t lateness = local_time_now -
                 conn->first_packet_time_to_play; if (lateness > 0) { debug(1,
                 "First packet is %" PRId64 " nanoseconds late! Flushing 0.5
                 seconds", lateness);

                            }
              */
            }

            if (conn->first_packet_time_to_play != 0) {
              // Now that we know the timing of the first packet...
              if (config.output->delay) {
                // and that the output device is capable of synchronization...

                // We may send packets of
                // silence from now until the time the first audio packet should
                // be sent and then we will send the first packet, which will be
                // followed by the subsequent packets. here, we figure out
                // whether and what silence to send.

                uint64_t should_be_time;

                // readjust first packet time to play
                frame_to_local_time(conn->first_packet_timestamp, // this will go modulo 2^32
                                    &should_be_time, conn);

                int64_t change_in_should_be_time =
                    (int64_t)(should_be_time - conn->first_packet_time_to_play);

                if (fabs(0.000001 * change_in_should_be_time) >
                    0.001) // the clock drift estimation might be nudging the
                           // estimate, and we can ignore this unless if's more
                           // than a microsecond
                  debug(2,
                        "Change in estimated first_packet_time: %f "
                        "milliseconds for first_packet .",
                        0.000001 * change_in_should_be_time);

                conn->first_packet_time_to_play = should_be_time;

                int64_t lead_time =
                    conn->first_packet_time_to_play - local_time_now; // negative means late
                if (lead_time < 0) {
                  debug(1, "Gone past starting time for %u by %" PRId64 " nanoseconds.",
                        conn->first_packet_timestamp, -lead_time);
                  conn->ab_buffering = 0;
                } else {
                  // do some calculations
                  if ((config.audio_backend_silent_lead_in_time_auto == 1) ||
                      (lead_time <= (int64_t)(config.audio_backend_silent_lead_in_time *
                                              (int64_t)1000000000))) {
                    // debug(1, "Lead time: %" PRId64 " nanoseconds.",
                    // lead_time);
                    int resp = 0;
                    dac_delay = 0;
                    if (have_sent_prefiller_silence != 0)
                      resp = config.output->delay(&dac_delay); // we know the output device must
                                                               // have a delay function
                    if (resp == 0) {
                      int64_t gross_frame_gap =
                          ((conn->first_packet_time_to_play - local_time_now) *
                           config.output_rate) /
                          1000000000;
                      int64_t exact_frame_gap = gross_frame_gap - dac_delay;
                      int64_t frames_needed_to_maintain_desired_buffer =
                          (int64_t)(config.audio_backend_buffer_desired_length *
                                    config.output_rate) -
                          dac_delay;
                      // below, remember that exact_frame_gap and
                      // frames_needed_to_maintain_desired_buffer could both be
                      // negative
                      int64_t fs = frames_needed_to_maintain_desired_buffer;

                      // if there isn't enough time to have the desired buffer
                      // size
                      if (exact_frame_gap <= frames_needed_to_maintain_desired_buffer) {
                        fs = conn->max_frames_per_packet * 2;
                      }
                      // if we are very close to the end of buffering, i.e.
                      // within two frame-lengths, add the remaining silence
                      // needed and end buffering
                      if (exact_frame_gap <= conn->max_frames_per_packet * 2) {
                        fs = exact_frame_gap;
                        if (fs > first_frame_early_bias)
                          fs = fs - first_frame_early_bias; // deliberately make the
                                                            // first packet a tiny
                                                            // bit early so that the
                                                            // player may compensate
                                                            // for it at the last
                                                            // minute
                        conn->ab_buffering = 0;
                      }
                      void *silence;
                      if (fs > 0) {
                        silence = malloc(conn->output_bytes_per_frame * fs);
                        if (silence == NULL)
                          debug(1, "Failed to allocate %d byte silence buffer.", fs);
                        else {
                          // generate frames of silence with dither if necessary
                          conn->previous_random_number = generate_zero_frames(
                              silence, fs, config.output_format, conn->enable_dither,
                              conn->previous_random_number);
                          config.output->play(silence, fs);
                          debug(2, "Sent %" PRId64 " frames of silence", fs);
                          free(silence);
                          have_sent_prefiller_silence = 1;
                        }
                      }
                    } else {
                      if (resp == sps_extra_code_output_stalled) {
                        if (conn->unfixable_error_reported == 0) {
                          conn->unfixable_error_reported = 1;

                          die("an unrecoverable error, "
                              "\"output_device_stalled\", has been "
                              "detected.",
                              conn->connection_number);
                        }
                      } else {
                        debug(3, "Unexpected response to getting dac delay: %d.", resp);
                      }
                    }
                  }
                }
              } else {
                // if the output device doesn't have a delay, we simply send the
                // lead-in
                int64_t lead_time =
                    conn->first_packet_time_to_play - local_time_now; // negative if we are late
                void *silence;
                int64_t frame_gap = (lead_time * config.output_rate) / 1000000000;
                // debug(1,"%d frames needed.",frame_gap);
                while (frame_gap > 0) {
                  ssize_t fs = config.output_rate / 10;
                  if (fs > frame_gap)
                    fs = frame_gap;

                  silence = malloc(conn->output_bytes_per_frame * fs);
                  if (silence == NULL)
                    debug(1, "Failed to allocate %d frame silence buffer.", fs);
                  else {
                    // debug(1, "No delay function -- outputting %d frames of
                    // silence.", fs);
                    conn->previous_random_number =
                        generate_zero_frames(silence, fs, config.output_format,
                                             conn->enable_dither, conn->previous_random_number);
                    config.output->play(silence, fs);
                    free(silence);
                  }
                  frame_gap -= fs;
                }
                conn->ab_buffering = 0;
              }
            }
          }
        }
      }

      // Here, we work out whether to release a packet or wait
      // We release a packet when the time is right.

      // To work out when the time is right, we need to take account of (1) the
      // actual time the packet should be released, (2) the latency requested,
      // (3) the audio backend latency offset and (4) the desired length of the
      // audio backend's buffer

      // The time is right if the current time is later or the same as
      // The packet time + (latency + latency offset - backend_buffer_length).
      // Note: the last three items are expressed in frames and must be
      // converted to time.

      int do_wait = 0; // don't wait unless we can really prove we must
      if ((conn->ab_synced) && (curframe) && (curframe->ready) && (curframe->given_timestamp)) {
        do_wait = 1; // if the current frame exists and is ready, then wait
                     // unless it's time to let it go...

        // here, get the time to play the current frame.

        if (have_timestamp_timing_information(conn)) { // if we have a reference time

          uint64_t time_to_play;

          // we must enable packets to be released early enough for the
          // audio buffer to be filled to the desired length

          uint32_t buffer_latency_offset =
              (uint32_t)(config.audio_backend_buffer_desired_length * conn->input_rate);
          frame_to_local_time(curframe->given_timestamp -
                                  buffer_latency_offset, // this will go modulo 2^32
                              &time_to_play, conn);

          if (local_time_now >= time_to_play) {
            do_wait = 0;
          }
          // here, do a sanity check. if the time_to_play is not within a few
          // seconds of the time now, the frame is probably not meant to be
          // there, so let it go.
          if (do_wait != 0) {
            // this is a hack.
            // we subtract two 2^n unsigned numbers and get a signed 2^n result.
            // If we think of the calculation as occurring in modulo 2^n
            // arithmetic then the signed result's magnitude represents the
            // shorter distance around the modulo wheel of values from one
            // number to the other. The sign indicates the direction: positive
            // means clockwise (upwards) from the second number to the first
            // (i.e. the first number comes "after" the second).

            int64_t time_difference = local_time_now - time_to_play;
            if ((time_difference > 10000000000) || (time_difference < -10000000000)) {
              debug(2,
                    "crazy time interval of %f seconds between time now: "
                    "0x%" PRIx64 " and time of packet: %" PRIx64 ".",
                    0.000000001 * time_difference, local_time_now, time_to_play);
              debug(2, "packet rtptime: %u, reference_timestamp: %u", curframe->given_timestamp,
                    conn->anchor_rtptime);

              do_wait = 0; // let it go
            }
          }
        }
      }
      if (do_wait == 0)
        if ((conn->ab_synced != 0) && (conn->ab_read == conn->ab_write)) { // the buffer is empty!
          if (notified_buffer_empty == 0) {
            debug(3, "Buffers exhausted.");
            notified_buffer_empty = 1;
            // reset_input_flow_metrics(conn); // don't do a full flush
            // parameters reset
            conn->initial_reference_time = 0;
            conn->initial_reference_timestamp = 0;
          }
          do_wait = 1;
        }
      wait = (conn->ab_buffering || (do_wait != 0) || (!conn->ab_synced));
    } else {
      wait = 1; // keep waiting until the timing information becomes available
    }
    if (wait) {
      if (conn->input_rate == 0)
        die("input_rate is zero -- should never happen!");
      uint64_t time_to_wait_for_wakeup_ns =
          1000000000 / conn->input_rate;     // this is time period of one frame
      time_to_wait_for_wakeup_ns *= 2 * 352; // two full 352-frame packets
      time_to_wait_for_wakeup_ns /= 3;       // two thirds of a packet time

      uint64_t time_of_wakeup_ns = get_realtime_in_ns() + time_to_wait_for_wakeup_ns;
      uint64_t sec = time_of_wakeup_ns / 1000000000;
      uint64_t nsec = time_of_wakeup_ns % 1000000000;

      struct timespec time_of_wakeup;
      time_of_wakeup.tv_sec = sec;
      time_of_wakeup.tv_nsec = nsec;
      //      pthread_cond_timedwait(&conn->flowcontrol, &conn->ab_mutex,
      //      &time_of_wakeup);
      int rc = pthread_cond_timedwait(&conn->flowcontrol, &conn->ab_mutex,
                                      &time_of_wakeup); // this is a pthread cancellation point
      if ((rc != 0) && (rc != ETIMEDOUT))
        debug(3, "pthread_cond_timedwait returned error code %d.", rc);
    }
  } while (wait);

  // seq_t read = conn->ab_read;
  if (curframe) {
    if (!curframe->ready) {
      // debug(1, "Supplying a silent frame for frame %u", read);
      conn->missing_packets++;
      curframe->given_timestamp = 0; // indicate a silent frame should be substituted
    }
    curframe->ready = 0;
  }
  conn->ab_read++;
  pthread_cleanup_pop(1);
  return curframe;
}

int64_t generate_zero_frames(char *outp, size_t number_of_frames, sps_format_t format,
                             int with_dither, int64_t random_number_in) {
  // return the last random number used
  // assuming the buffer has been assigned

  // add a TPDF dither -- see
  // http://educypedia.karadimov.info/library/DitherExplained.pdf
  // and the discussion around
  // https://www.hydrogenaud.io/forums/index.php?showtopic=16963&st=25

  // I think, for a 32 --> 16 bits, the range of
  // random numbers needs to be from -2^16 to 2^16, i.e. from -65536 to 65536
  // inclusive, not from -32768 to +32767

  // Actually, what would be generated here is from -65535 to 65535, i.e. one
  // less on the limits.

  // See the original paper at
  // http://www.ece.rochester.edu/courses/ECE472/resources/Papers/Lipshitz_1992.pdf
  // by Lipshitz, Wannamaker and Vanderkooy, 1992.

  int64_t dither_mask = 0;
  switch (format) {
    case SPS_FORMAT_S32:
    case SPS_FORMAT_S32_LE:
    case SPS_FORMAT_S32_BE:
      dither_mask = (int64_t)1 << (64 - 32);
      break;
    case SPS_FORMAT_S24:
    case SPS_FORMAT_S24_LE:
    case SPS_FORMAT_S24_BE:
    case SPS_FORMAT_S24_3LE:
    case SPS_FORMAT_S24_3BE:
      dither_mask = (int64_t)1 << (64 - 24);
      break;
    case SPS_FORMAT_S16:
    case SPS_FORMAT_S16_LE:
    case SPS_FORMAT_S16_BE:
      dither_mask = (int64_t)1 << (64 - 16);
      break;

    case SPS_FORMAT_S8:

    case SPS_FORMAT_U8:
      dither_mask = (int64_t)1 << (64 - 8);
      break;
    case SPS_FORMAT_UNKNOWN:
      die("Unexpected SPS_FORMAT_UNKNOWN while calculating dither mask.");
      break;
    case SPS_FORMAT_AUTO:
      die("Unexpected SPS_FORMAT_AUTO while calculating dither mask.");
      break;
    case SPS_FORMAT_INVALID:
      die("Unexpected SPS_FORMAT_INVALID while calculating dither mask.");
      break;
  }
  dither_mask -= 1;

  int64_t previous_random_number = random_number_in;
  char *p = outp;
  size_t sample_number;
  r64_lock; // the random number generator is not thread safe, so we need to
            // lock it while using it
  for (sample_number = 0; sample_number < number_of_frames * 2; sample_number++) {
    int64_t hyper_sample = 0;
    int64_t r = r64i();

    int64_t tpdf = (r & dither_mask) - (previous_random_number & dither_mask);

    // add dither if permitted -- no need to check for clipping, as the sample
    // is, uh, zero

    if (with_dither != 0)
      hyper_sample += tpdf;

    // move the result to the desired position in the int64_t
    char *op = p;
    int sample_length; // this is the length of the sample

    switch (format) {
      case SPS_FORMAT_S32:
        hyper_sample >>= (64 - 32);
        *(int32_t *)op = hyper_sample;
        sample_length = 4;
        break;
      case SPS_FORMAT_S32_LE:
        *op++ = (uint8_t)(hyper_sample >> (64 - 32));      // 32 bits, ls byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 32 + 8));  // 32 bits, less significant middle byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 32 + 16)); // 32 bits, more significant middle byte
        *op = (uint8_t)(hyper_sample >> (64 - 32 + 24));   // 32 bits, ms byte
        sample_length = 4;
        break;
      case SPS_FORMAT_S32_BE:
        *op++ = (uint8_t)(hyper_sample >> (64 - 32 + 24)); // 32 bits, ms byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 32 + 16)); // 32 bits, more significant middle byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 32 + 8));  // 32 bits, less significant middle byte
        *op = (uint8_t)(hyper_sample >> (64 - 32));        // 32 bits, ls byte
        sample_length = 4;
        break;
      case SPS_FORMAT_S24_3LE:
        *op++ = (uint8_t)(hyper_sample >> (64 - 24));     // 24 bits, ls byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8)); // 24 bits, middle byte
        *op = (uint8_t)(hyper_sample >> (64 - 24 + 16));  // 24 bits, ms byte
        sample_length = 3;
        break;
      case SPS_FORMAT_S24_3BE:
        *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 16)); // 24 bits, ms byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8));  // 24 bits, middle byte
        *op = (uint8_t)(hyper_sample >> (64 - 24));        // 24 bits, ls byte
        sample_length = 3;
        break;
      case SPS_FORMAT_S24:
        hyper_sample >>= (64 - 24);
        *(int32_t *)op = hyper_sample;
        sample_length = 4;
        break;
      case SPS_FORMAT_S24_LE:
        *op++ = (uint8_t)(hyper_sample >> (64 - 24));      // 24 bits, ls byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8));  // 24 bits, middle byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 16)); // 24 bits, ms byte
        *op = 0;
        sample_length = 4;
        break;
      case SPS_FORMAT_S24_BE:
        *op++ = 0;
        *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 16)); // 24 bits, ms byte
        *op++ = (uint8_t)(hyper_sample >> (64 - 24 + 8));  // 24 bits, middle byte
        *op = (uint8_t)(hyper_sample >> (64 - 24));        // 24 bits, ls byte
        sample_length = 4;
        break;
      case SPS_FORMAT_S16_LE:
        *op++ = (uint8_t)(hyper_sample >> (64 - 16));
        *op++ = (uint8_t)(hyper_sample >> (64 - 16 + 8)); // 16 bits, ms byte
        sample_length = 2;
        break;
      case SPS_FORMAT_S16_BE:
        *op++ = (uint8_t)(hyper_sample >> (64 - 16 + 8)); // 16 bits, ms byte
        *op = (uint8_t)(hyper_sample >> (64 - 16));
        sample_length = 2;
        break;
      case SPS_FORMAT_S16:
        *(int16_t *)op = (int16_t)(hyper_sample >> (64 - 16));
        sample_length = 2;
        break;
      case SPS_FORMAT_S8:
        *op = (int8_t)(hyper_sample >> (64 - 8));
        sample_length = 1;
        break;
      case SPS_FORMAT_U8:
        *op = 128 + (uint8_t)(hyper_sample >> (64 - 8));
        sample_length = 1;
        break;
      default:
        sample_length = 0; // stop a compiler warning
        die("Unexpected SPS_FORMAT_* with index %d while outputting silence", format);
    }
    p += sample_length;
    previous_random_number = r;
  }
  r64_unlock;
  return previous_random_number;
}

void reset_input_flow_metrics(rtsp_conn_info *conn) {
  conn->play_number_after_flush = 0;
  conn->packet_count_since_flush = 0;
  conn->input_frame_rate_starting_point_is_valid = 0;
  conn->initial_reference_time = 0;
  conn->initial_reference_timestamp = 0;
}

void *rtp_audio_receiver(void *arg) {
  debug(3, "rtp_audio_receiver start");
  pthread_cleanup_push(rtp_audio_receiver_cleanup_handler, arg);
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;

  int32_t last_seqno = -1;
  uint8_t packet[2048], *pktp;

  uint64_t time_of_previous_packet_ns = 0;
  float longest_packet_time_interval_us = 0.0;

  // mean and variance calculations from "online_variance" algorithm at
  // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm

  int32_t stat_n = 0;
  float stat_mean = 0.0;
  float stat_M2 = 0.0;

  int frame_count = 0;
  ssize_t nread;
  while (1) {
    nread = recv(conn->audio_socket, packet, sizeof(packet), 0);

    frame_count++;

    uint64_t local_time_now_ns = get_absolute_time_in_ns();
    if (time_of_previous_packet_ns) {
      float time_interval_us = (local_time_now_ns - time_of_previous_packet_ns) * 0.001;
      time_of_previous_packet_ns = local_time_now_ns;
      if (time_interval_us > longest_packet_time_interval_us)
        longest_packet_time_interval_us = time_interval_us;
      stat_n += 1;
      float stat_delta = time_interval_us - stat_mean;
      stat_mean += stat_delta / stat_n;
      stat_M2 += stat_delta * (time_interval_us - stat_mean);
      if ((stat_n != 1) && (stat_n % 2500 == 0)) {
        debug(2,
              "Packet reception interval stats: mean, standard deviation and "
              "max for the last "
              "2,500 packets in microseconds: %10.1f, %10.1f, %10.1f.",
              stat_mean, sqrtf(stat_M2 / (stat_n - 1)), longest_packet_time_interval_us);
        stat_n = 0;
        stat_mean = 0.0;
        stat_M2 = 0.0;
        time_of_previous_packet_ns = 0;
        longest_packet_time_interval_us = 0.0;
      }
    } else {
      time_of_previous_packet_ns = local_time_now_ns;
    }

    if (nread >= 0) {
      ssize_t plen = nread;
      uint8_t type = packet[1] & ~0x80;
      if (type == 0x60 || type == 0x56) { // audio data / resend
        pktp = packet;
        if (type == 0x56) {
          pktp += 4;
          plen -= 4;
        }
        seq_t seqno = ntohs(*(uint16_t *)(pktp + 2));
        // increment last_seqno and see if it's the same as the incoming seqno

        if (type == 0x60) { // regular audio data

          /*
          char obf[4096];
          char *obfp = obf;
          int obfc;
          for (obfc=0;obfc<plen;obfc++) {
            snprintf(obfp, 3, "%02X", pktp[obfc]);
            obfp+=2;
          };
          *obfp=0;
          debug(1,"Audio Packet Received: \"%s\"",obf);
          */

          if (last_seqno == -1)
            last_seqno = seqno;
          else {
            last_seqno = (last_seqno + 1) & 0xffff;
            // if (seqno != last_seqno)
            //  debug(3, "RTP: Packets out of sequence: expected: %d, got %d.",
            //  last_seqno, seqno);
            last_seqno = seqno; // reset warning...
          }
        } else {
          debug(3, "Audio Receiver -- Retransmitted Audio Data Packet %u received.", seqno);
        }

        uint32_t actual_timestamp = ntohl(*(uint32_t *)(pktp + 4));

        // uint32_t ssid = ntohl(*(uint32_t *)(pktp + 8));
        // debug(1, "Audio packet SSID: %08X,%u", ssid,ssid);

        // if (packet[1]&0x10)
        //	debug(1,"Audio packet Extension bit set.");

        pktp += 12;
        plen -= 12;

        // check if packet contains enough content to be reasonable
        if (plen >= 16) {
          player_put_packet(1, seqno, actual_timestamp, pktp, plen,
                            conn); // the '1' means is original format
        }

        if (type == 0x56 && seqno == 0) {
          debug(2, "resend-related request packet received, ignoring.");
          continue;
        }
        debug(1,
              "Audio receiver -- Unknown RTP packet of type 0x%02X length %d "
              "seqno %d",
              type, nread, seqno);
      }
      warn("Audio receiver -- Unknown RTP packet of type 0x%02X length %d.", type, nread);
    } else {
      char em[1024];
      strerror_r(errno, em, sizeof(em));
      debug(1, "Error %d receiving an audio packet: \"%s\".", errno, em);
    }
  }

  /*
  debug(3, "Audio receiver -- Server RTP thread interrupted. terminating.");
  close(conn->audio_socket);
  */

  debug(1, "Audio receiver thread \"normal\" exit -- this can't happen. Hah!");
  pthread_cleanup_pop(0); // don't execute anything here.
  debug(2, "Audio receiver thread exit.");
  pthread_exit(NULL);
}

void player_put_packet(int original_format, seq_t seqno, uint32_t actual_timestamp, uint8_t *data,
                       int len, rtsp_conn_info *conn) {
  // if it's original format, it has a valid seqno and must be decoded
  // otherwise, it can take the next seqno and doesn't need decoding.

  // ignore a request to flush that has been made before the first packet...
  if (conn->packet_count == 0) {
    debug_mutex_lock(&conn->flush_mutex, 1000, 1);
    conn->flush_requested = 0;
    conn->flush_rtp_timestamp = 0;
    debug_mutex_unlock(&conn->flush_mutex, 3);
  }

  debug_mutex_lock(&conn->ab_mutex, 30000, 0);
  uint64_t time_now = get_absolute_time_in_ns();
  conn->packet_count++;
  conn->packet_count_since_flush++;
  conn->time_of_last_audio_packet = time_now;
  if (conn->connection_state_to_output) { // if we are supposed to be processing
                                          // these packets
    abuf_t *abuf = 0;
    if (!conn->ab_synced) {
      conn->ab_write = seqno;
      conn->ab_read = seqno;
      conn->ab_synced = 1;
      conn->first_packet_timestamp = 0;
      debug(2, "Connection %d: synced by first packet, seqno %u.", conn->connection_number, seqno);
    } else if (original_format == 0) {
      // if the packet is coming in original format, the sequence number is
      // important otherwise, ignore is by setting it equal to the expected
      // sequence number in ab_write
      seqno = conn->ab_write;
    }
    if (conn->ab_write == seqno) { // if this is the expected packet (which
                                   // could be the first packet...)
      if (conn->input_frame_rate_starting_point_is_valid == 0) {
        if ((conn->packet_count_since_flush >= 500) && (conn->packet_count_since_flush <= 510)) {
          conn->frames_inward_measurement_start_time = time_now;
          conn->frames_inward_frames_received_at_measurement_start_time = actual_timestamp;
          conn->input_frame_rate_starting_point_is_valid = 1; // valid now
        }
      }
      conn->frames_inward_measurement_time = time_now;
      conn->frames_inward_frames_received_at_measurement_time = actual_timestamp;
      abuf = conn->audio_buffer + BUFIDX(seqno);
      conn->ab_write = seqno + 1;                 // move the write pointer to the next free space
    } else if (is_after(conn->ab_write, seqno)) { // newer than expected
      int32_t gap = seqno - conn->ab_write;
      if (gap <= 0)
        debug(1, "Unexpected gap size: %d.", gap);
      int i;
      for (i = 0; i < gap; i++) {
        abuf = conn->audio_buffer + BUFIDX(conn->ab_write + i);
        abuf->ready = 0; // to be sure, to be sure
        abuf->resend_request_number = 0;
        abuf->initialisation_time = time_now; // this represents when the packet
                                              // was noticed to be missing
        abuf->status = 1 << 0;                // signifying missing
        abuf->resend_time = 0;
        abuf->given_timestamp = 0;
        abuf->sequence_number = 0;
      }
      abuf = conn->audio_buffer + BUFIDX(seqno);
      //        rtp_request_resend(ab_write, gap);
      //        resend_requests++;
      conn->ab_write = seqno + 1;
    } else if (is_after(conn->ab_read,
                        seqno)) { // older than expected but not too late
      conn->late_packets++;
      abuf = conn->audio_buffer + BUFIDX(seqno);
    } else { // too late.
      conn->too_late_packets++;
    }

    if (abuf) {
      int datalen = conn->max_frames_per_packet;
      abuf->initialisation_time = time_now;
      abuf->resend_time = 0;
      if ((original_format != 0) &&
          (audio_packet_decode(abuf->data, &datalen, data, len, conn) == 0)) {
        abuf->ready = 1;
        abuf->status = 0; // signifying that it was received
        abuf->length = datalen;
        abuf->given_timestamp = actual_timestamp;
        abuf->sequence_number = seqno;
      } else if (original_format == 0) {
        memcpy(abuf->data, data, len * conn->input_bytes_per_frame);
        abuf->ready = 1;
        abuf->status = 0; // signifying that it was received
        abuf->length = len;
        abuf->given_timestamp = actual_timestamp;
        abuf->sequence_number = seqno;
      } else {
        debug(1, "Bad audio packet detected and discarded.");
        abuf->ready = 0;
        abuf->status = 1 << 1; // bad packet, discarded
        abuf->resend_request_number = 0;
        abuf->given_timestamp = 0;
        abuf->sequence_number = 0;
      }
    }

    int rc = pthread_cond_signal(&conn->flowcontrol);
    if (rc)
      debug(1, "Error signalling flowcontrol.");

    // resend checks
    {
      uint64_t minimum_wait_time =
          (uint64_t)(config.resend_control_first_check_time * (uint64_t)1000000000);
      uint64_t resend_repeat_interval =
          (uint64_t)(config.resend_control_check_interval_time * (uint64_t)1000000000);
      uint64_t minimum_remaining_time = (uint64_t)((config.resend_control_last_check_time +
                                                    config.audio_backend_buffer_desired_length) *
                                                   (uint64_t)1000000000);
      uint64_t latency_time = (uint64_t)(conn->latency * (uint64_t)1000000000);
      latency_time = latency_time / (uint64_t)conn->input_rate;

      // find the first frame that is missing, if known
      int x = conn->ab_read;
      if (first_possibly_missing_frame >= 0) {
        // if it's within the range
        int16_t buffer_size = conn->ab_write - conn->ab_read; // must be positive
        if (buffer_size >= 0) {
          int16_t position_in_buffer = first_possibly_missing_frame - conn->ab_read;
          if ((position_in_buffer >= 0) && (position_in_buffer < buffer_size))
            x = first_possibly_missing_frame;
        }
      }

      first_possibly_missing_frame = -1; // has not been set

      int missing_frame_run_count = 0;
      int start_of_missing_frame_run = -1;
      int number_of_missing_frames = 0;
      while (x != conn->ab_write) {
        abuf_t *check_buf = conn->audio_buffer + BUFIDX(x);
        if (!check_buf->ready) {
          if (first_possibly_missing_frame < 0)
            first_possibly_missing_frame = x;
          number_of_missing_frames++;
          // debug(1, "frame %u's initialisation_time is 0x%" PRIx64 ",
          // latency_time is 0x%" PRIx64 ", time_now is 0x%" PRIx64 ",
          // minimum_remaining_time is 0x%" PRIx64 ".", x,
          // check_buf->initialisation_time, latency_time, time_now,
          // minimum_remaining_time);
          int too_late = ((check_buf->initialisation_time < (time_now - latency_time)) ||
                          ((check_buf->initialisation_time - (time_now - latency_time)) <
                           minimum_remaining_time));
          int too_early = ((time_now - check_buf->initialisation_time) < minimum_wait_time);
          int too_soon_after_last_request = ((check_buf->resend_time != 0) &&
                                             ((time_now - check_buf->resend_time) <
                                              resend_repeat_interval)); // time_now can never be
                                                                        // less than the time_tag

          if (too_late)
            check_buf->status |= 1 << 2; // too late
          else
            check_buf->status &= 0xFF - (1 << 2); // not too late
          if (too_early)
            check_buf->status |= 1 << 3; // too early
          else
            check_buf->status &= 0xFF - (1 << 3); // not too early
          if (too_soon_after_last_request)
            check_buf->status |= 1 << 4; // too soon after last request
          else
            check_buf->status &= 0xFF - (1 << 4); // not too soon after last request

          if ((!too_soon_after_last_request) && (!too_late) && (!too_early)) {
            if (start_of_missing_frame_run == -1) {
              start_of_missing_frame_run = x;
              missing_frame_run_count = 1;
            } else {
              missing_frame_run_count++;
            }
            check_buf->resend_time = time_now; // setting the time to now because we are
                                               // definitely going to take action
            check_buf->resend_request_number++;
            debug(3, "Frame %d is missing with ab_read of %u and ab_write of %u.", x,
                  conn->ab_read, conn->ab_write);
          }
          // if (too_late) {
          //   debug(1,"too late to get missing frame %u.", x);
          // }
        }
        // if (number_of_missing_frames != 0)
        //  debug(1,"check with x = %u, ab_read = %u, ab_write = %u,
        //  first_possibly_missing_frame = %d.", x, conn->ab_read,
        //  conn->ab_write, first_possibly_missing_frame);
        x = (x + 1) & 0xffff;
        if (((check_buf->ready) || (x == conn->ab_write)) && (missing_frame_run_count > 0)) {
          // send a resend request
          if (missing_frame_run_count > 1)
            debug(3, "request resend of %d packets starting at seqno %u.", missing_frame_run_count,
                  start_of_missing_frame_run);
          if (config.disable_resend_requests == 0) {
            debug_mutex_unlock(&conn->ab_mutex, 3);
            rtp_request_resend(start_of_missing_frame_run, missing_frame_run_count, conn);
            debug_mutex_lock(&conn->ab_mutex, 20000, 1);
            conn->resend_requests++;
          }
          start_of_missing_frame_run = -1;
          missing_frame_run_count = 0;
        }
      }
      if (number_of_missing_frames == 0)
        first_possibly_missing_frame = conn->ab_write;
    }
  }
  debug_mutex_unlock(&conn->ab_mutex, 0);
}

// the sequence numbers will wrap pretty often.
// this returns true if the second arg is strictly after the first
static inline int is_after(seq_t a, seq_t b) {
  int16_t d = b - a;
  return d > 0;
}

int audio_packet_decode(short *dest, int *destlen, uint8_t *buf, int len, rtsp_conn_info *conn) {
  // parameters: where the decoded stuff goes, its length in samples,
  // the incoming packet, the length of the incoming packet in bytes
  // destlen should contain the allowed max number of samples on entry

  if (len > MAX_PACKET) {
    warn("Incoming audio packet size is too large at %d; it should not exceed "
         "%d.",
         len, MAX_PACKET);
    return -1;
  }
  unsigned char packet[MAX_PACKET];
  // unsigned char packetp[MAX_PACKET];
  assert(len <= MAX_PACKET);
  int reply = 0; // everything okay
  int outsize =
      conn->input_bytes_per_frame * (*destlen); // the size the output should be, in bytes
  int maximum_possible_outsize = outsize;

  if (conn->stream.encrypted) {
    unsigned char iv[16];
    int aeslen = len & ~0xf;
    memcpy(iv, conn->stream.aesiv, sizeof(iv));

    AES_cbc_encrypt(buf, packet, aeslen, &conn->aes, iv, AES_DECRYPT);

    memcpy(packet + aeslen, buf + aeslen, len - aeslen);
    unencrypted_packet_decode(packet, len, dest, &outsize, maximum_possible_outsize, conn);
  } else {
    // not encrypted
    unencrypted_packet_decode(buf, len, dest, &outsize, maximum_possible_outsize, conn);
  }

  if (outsize > maximum_possible_outsize) {
    debug(2,
          "Output from alac_decode larger (%d bytes, not frames) than expected "
          "(%d bytes) -- "
          "truncated, but buffer overflow possible! Encrypted = %d.",
          outsize, maximum_possible_outsize, conn->stream.encrypted);
    reply = -1; // output packet is the wrong size
  }

  if (conn->input_bytes_per_frame != 0)
    *destlen = outsize / conn->input_bytes_per_frame;
  else
    die("Unexpectedly, conn->input_bytes_per_frame is zero.");
  if ((outsize % conn->input_bytes_per_frame) != 0)
    debug(1,
          "Number of audio frames (%d) does not correspond exactly to the "
          "number of bytes (%d) "
          "and the audio frame size (%d).",
          *destlen, outsize, conn->input_bytes_per_frame);
  return reply;
}

void rtp_request_resend(seq_t first, uint32_t count, rtsp_conn_info *conn) {
  // debug(1, "rtp_request_resend of %u packets from sequence number %u.",
  // count, first);
  if (conn->rtp_running) {
    // if (!request_sent) {
    // debug(2, "requesting resend of %d packets starting at %u.", count,
    // first);
    //  request_sent = 1;
    //}

    char req[8]; // *not* a standard RTCP NACK
    req[0] = 0x80;

    if (conn->airplay_type == ap_2) {
      if (conn->ap2_remote_control_socket_addr_length == 0) {
        debug(2, "No remote socket -- skipping the resend");
        return; // hack
      }
      req[1] = 0xD5; // Airplay 2 'resend'
    } else {
      req[1] = (char)0x55 | (char)0x80; // Apple 'resend'
    }

    *(unsigned short *)(req + 2) = htons(1);     // our sequence number
    *(unsigned short *)(req + 4) = htons(first); // missed seqnum
    *(unsigned short *)(req + 6) = htons(count); // count

    uint64_t time_of_sending_ns = get_absolute_time_in_ns();
    uint64_t resend_error_backoff_time = 300000000; // 0.3 seconds
    if ((conn->rtp_time_of_last_resend_request_error_ns == 0) ||
        ((time_of_sending_ns - conn->rtp_time_of_last_resend_request_error_ns) >
         resend_error_backoff_time)) {
      // put a time limit on the sendto

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;
      int response;

      if (conn->airplay_type == ap_2) {
        if (setsockopt(conn->ap2_control_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                       sizeof(timeout)) < 0)
          debug(1, "Can't set timeout on resend request socket.");
        response = sendto(conn->ap2_control_socket, req, sizeof(req), 0,
                          (struct sockaddr *)&conn->ap2_remote_control_socket_addr,
                          conn->ap2_remote_control_socket_addr_length);
      } else {
        if (setsockopt(conn->control_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                       sizeof(timeout)) < 0)
          debug(1, "Can't set timeout on resend request socket.");
        socklen_t msgsize = sizeof(struct sockaddr_in);
#ifdef AF_INET6
        if (conn->rtp_client_control_socket.SAFAMILY == AF_INET6) {
          msgsize = sizeof(struct sockaddr_in6);
        }
#endif
        response = sendto(conn->control_socket, req, sizeof(req), 0,
                          (struct sockaddr *)&conn->rtp_client_control_socket, msgsize);
      }

      if (response == -1) {
        char em[1024];
        strerror_r(errno, em, sizeof(em));
        debug(2, "Error %d using sendto to request a resend: \"%s\".", errno, em);
        conn->rtp_time_of_last_resend_request_error_ns = time_of_sending_ns;
      } else {
        conn->rtp_time_of_last_resend_request_error_ns = 0;
      }
    } else {
      debug(1, "Suppressing a resend request due to a resend sendto error in "
               "the last 0.3 seconds.");
    }
  } else {
    // if (!request_sent) {
    debug(2, "rtp_request_resend called without active stream!");
    //  request_sent = 1;
    //}
  }
}

void unencrypted_packet_decode(unsigned char *packet, int length, short *dest, int *outsize,
                               int size_limit, rtsp_conn_info *conn) {
  if (conn->stream.type == ast_apple_lossless) {
    {
      if (conn->decoder_in_use != 1 << decoder_hammerton) {
        debug(2, "Hammerton Decoder used on encrypted audio.");
        conn->decoder_in_use = 1 << decoder_hammerton;
      }
      alac_decode_frame(conn->decoder_info, packet, (unsigned char *)dest, outsize);
    }
  } else if (conn->stream.type == ast_uncompressed) {
    int length_to_use = length;
    if (length_to_use > size_limit) {
      warn("unencrypted_packet_decode: uncompressed audio packet too long "
           "(size: %d bytes) to "
           "process -- truncated",
           length);
      length_to_use = size_limit;
    }
    int i;
    short *source = (short *)packet;
    for (i = 0; i < (length_to_use / 2); i++) {
      *dest = ntohs(*source);
      dest++;
      source++;
    }
    *outsize = length_to_use;
  }
}

void alac_decode_frame(alac_file *alac, unsigned char *inbuffer, void *outbuffer,
                       int *outputsize) {
  int outbuffer_allocation_size = *outputsize; // initial value
  int channels;
  int32_t outputsamples = alac->setinfo_max_samples_per_frame;

  /* setup the stream */
  alac->input_buffer = inbuffer;
  alac->input_buffer_bitaccumulator = 0;

  channels = readbits(alac, 3);

  *outputsize = outputsamples * alac->bytespersample;
  if (*outputsize > outbuffer_allocation_size) {
    fprintf(stderr, "FIXME: Not enough space if the output buffer for audio frame - E1.\n");
    *outputsize = 0;
    return;
  }

  switch (channels) {
    case 0: /* 1 channel */
    {
      int hassize;
      int isnotcompressed;
      int readsamplesize;

      int uncompressed_bytes;
      int ricemodifier;

      /* 2^result = something to do with output waiting.
       * perhaps matters if we read > 1 frame in a pass?
       */
      readbits(alac, 4);

      readbits(alac, 12); /* unknown, skip 12 bits */

      hassize = readbits(alac, 1); /* the output sample size is stored soon */

      uncompressed_bytes = readbits(
          alac, 2); /* number of bytes in the (compressed) stream that are not compressed */

      isnotcompressed = readbits(alac, 1); /* whether the frame is compressed */

      if (hassize) {
        /* now read the number of samples,
         * as a 32bit integer */
        outputsamples = readbits(alac, 32);
        *outputsize = outputsamples * alac->bytespersample;
        if (*outputsize > outbuffer_allocation_size) {
          fprintf(stderr, "FIXME: Not enough space if the output buffer for audio frame - E2.\n");
          *outputsize = 0;
          return;
        }
      }

      readsamplesize = alac->setinfo_sample_size - (uncompressed_bytes * 8);

      if (!isnotcompressed) { /* so it is compressed */
        int16_t predictor_coef_table[32];
        int predictor_coef_num;
        int prediction_type;
        int prediction_quantitization;
        int i;

        /* skip 16 bits, not sure what they are. seem to be used in
         * two channel case */
        readbits(alac, 8);
        readbits(alac, 8);

        prediction_type = readbits(alac, 4);
        prediction_quantitization = readbits(alac, 4);

        ricemodifier = readbits(alac, 3);
        predictor_coef_num = readbits(alac, 5);

        /* read the predictor table */
        for (i = 0; i < predictor_coef_num; i++) {
          predictor_coef_table[i] = (int16_t)readbits(alac, 16);
        }

        if (uncompressed_bytes) {
          int i;
          for (i = 0; i < outputsamples; i++) {
            alac->uncompressed_bytes_buffer_a[i] = readbits(alac, uncompressed_bytes * 8);
          }
        }

        entropy_rice_decode(alac, alac->predicterror_buffer_a, outputsamples, readsamplesize,
                            alac->setinfo_rice_initialhistory, alac->setinfo_rice_kmodifier,
                            ricemodifier * alac->setinfo_rice_historymult / 4,
                            (1 << alac->setinfo_rice_kmodifier) - 1);

        if (prediction_type == 0) { /* adaptive fir */
          predictor_decompress_fir_adapt(alac->predicterror_buffer_a, alac->outputsamples_buffer_a,
                                         outputsamples, readsamplesize, predictor_coef_table,
                                         predictor_coef_num, prediction_quantitization);
        } else {
          fprintf(stderr, "FIXME: unhandled prediction type for compressed case: %i\n",
                  prediction_type);
          /* i think the only other prediction type (or perhaps this is just a
           * boolean?) runs adaptive fir twice.. like:
           * predictor_decompress_fir_adapt(predictor_error, tempout, ...)
           * predictor_decompress_fir_adapt(predictor_error, outputsamples ...)
           * little strange..
           */
        }
      } else { /* not compressed, easy case */
        if (alac->setinfo_sample_size <= 16) {
          int i;
          for (i = 0; i < outputsamples; i++) {
            int32_t audiobits = readbits(alac, alac->setinfo_sample_size);

            audiobits = SIGN_EXTENDED32(audiobits, alac->setinfo_sample_size);

            alac->outputsamples_buffer_a[i] = audiobits;
          }
        } else {
          int i;
          for (i = 0; i < outputsamples; i++) {
            int32_t audiobits;

            audiobits = readbits(alac, 16);
            /* special case of sign extension..
             * as we'll be ORing the low 16bits into this */
            audiobits = audiobits << (alac->setinfo_sample_size - 16);
            audiobits |= readbits(alac, alac->setinfo_sample_size - 16);
            audiobits = SignExtend24(audiobits);

            alac->outputsamples_buffer_a[i] = audiobits;
          }
        }
        uncompressed_bytes = 0; // always 0 for uncompressed
      }

      switch (alac->setinfo_sample_size) {
        case 16: {
          int i;
          for (i = 0; i < outputsamples; i++) {
            int16_t sample = alac->outputsamples_buffer_a[i];
            if (host_bigendian)
              _Swap16(sample);
            ((int16_t *)outbuffer)[i * alac->numchannels] = sample;
          }
          break;
        }
        case 24: {
          int i;
          for (i = 0; i < outputsamples; i++) {
            int32_t sample = alac->outputsamples_buffer_a[i];

            if (uncompressed_bytes) {
              uint32_t mask;
              sample = sample << (uncompressed_bytes * 8);
              mask = ~(0xFFFFFFFF << (uncompressed_bytes * 8));
              sample |= alac->uncompressed_bytes_buffer_a[i] & mask;
            }

            ((uint8_t *)outbuffer)[i * alac->numchannels * 3] = (sample)&0xFF;
            ((uint8_t *)outbuffer)[i * alac->numchannels * 3 + 1] = (sample >> 8) & 0xFF;
            ((uint8_t *)outbuffer)[i * alac->numchannels * 3 + 2] = (sample >> 16) & 0xFF;
          }
          break;
        }
        case 20:
        case 32:
          fprintf(stderr, "FIXME: unimplemented sample size %i\n", alac->setinfo_sample_size);
          break;
        default:
          break;
      }
      break;
    }
    case 1: /* 2 channels */
    {
      int hassize;
      int isnotcompressed;
      int readsamplesize;

      int uncompressed_bytes;

      uint8_t interlacing_shift;
      uint8_t interlacing_leftweight;

      /* 2^result = something to do with output waiting.
       * perhaps matters if we read > 1 frame in a pass?
       */
      readbits(alac, 4);

      readbits(alac, 12); /* unknown, skip 12 bits */

      hassize = readbits(alac, 1); /* the output sample size is stored soon */

      uncompressed_bytes = readbits(
          alac, 2); /* the number of bytes in the (compressed) stream that are not compressed */

      isnotcompressed = readbits(alac, 1); /* whether the frame is compressed */

      if (hassize) {
        /* now read the number of samples,
         * as a 32bit integer */
        outputsamples = readbits(alac, 32);
        *outputsize = outputsamples * alac->bytespersample;
        if (*outputsize > outbuffer_allocation_size) {
          fprintf(stderr, "FIXME: Not enough space if the output buffer for audio frame - E3.\n");
          *outputsize = 0;
          return;
        }
      }

      readsamplesize = alac->setinfo_sample_size - (uncompressed_bytes * 8) + 1;

      if (!isnotcompressed) { /* compressed */
        int16_t predictor_coef_table_a[32];
        int predictor_coef_num_a;
        int prediction_type_a;
        int prediction_quantitization_a;
        int ricemodifier_a;

        int16_t predictor_coef_table_b[32];
        int predictor_coef_num_b;
        int prediction_type_b;
        int prediction_quantitization_b;
        int ricemodifier_b;

        int i;

        interlacing_shift = readbits(alac, 8);
        interlacing_leftweight = readbits(alac, 8);

        /******** channel 1 ***********/
        prediction_type_a = readbits(alac, 4);
        prediction_quantitization_a = readbits(alac, 4);

        ricemodifier_a = readbits(alac, 3);
        predictor_coef_num_a = readbits(alac, 5);

        /* read the predictor table */
        for (i = 0; i < predictor_coef_num_a; i++) {
          predictor_coef_table_a[i] = (int16_t)readbits(alac, 16);
        }

        /******** channel 2 *********/
        prediction_type_b = readbits(alac, 4);
        prediction_quantitization_b = readbits(alac, 4);

        ricemodifier_b = readbits(alac, 3);
        predictor_coef_num_b = readbits(alac, 5);

        /* read the predictor table */
        for (i = 0; i < predictor_coef_num_b; i++) {
          predictor_coef_table_b[i] = (int16_t)readbits(alac, 16);
        }

        /*********************/
        if (uncompressed_bytes) { /* see mono case */
          int i;
          for (i = 0; i < outputsamples; i++) {
            alac->uncompressed_bytes_buffer_a[i] = readbits(alac, uncompressed_bytes * 8);
            alac->uncompressed_bytes_buffer_b[i] = readbits(alac, uncompressed_bytes * 8);
          }
        }

        /* channel 1 */
        entropy_rice_decode(alac, alac->predicterror_buffer_a, outputsamples, readsamplesize,
                            alac->setinfo_rice_initialhistory, alac->setinfo_rice_kmodifier,
                            ricemodifier_a * alac->setinfo_rice_historymult / 4,
                            (1 << alac->setinfo_rice_kmodifier) - 1);

        if (prediction_type_a == 0) { /* adaptive fir */
          predictor_decompress_fir_adapt(alac->predicterror_buffer_a, alac->outputsamples_buffer_a,
                                         outputsamples, readsamplesize, predictor_coef_table_a,
                                         predictor_coef_num_a, prediction_quantitization_a);
        } else { /* see mono case */
          fprintf(stderr, "FIXME: unhandled prediction type on channel 1: %i\n",
                  prediction_type_a);
        }

        /* channel 2 */
        entropy_rice_decode(alac, alac->predicterror_buffer_b, outputsamples, readsamplesize,
                            alac->setinfo_rice_initialhistory, alac->setinfo_rice_kmodifier,
                            ricemodifier_b * alac->setinfo_rice_historymult / 4,
                            (1 << alac->setinfo_rice_kmodifier) - 1);

        if (prediction_type_b == 0) { /* adaptive fir */
          predictor_decompress_fir_adapt(alac->predicterror_buffer_b, alac->outputsamples_buffer_b,
                                         outputsamples, readsamplesize, predictor_coef_table_b,
                                         predictor_coef_num_b, prediction_quantitization_b);
        } else {
          fprintf(stderr, "FIXME: unhandled prediction type on channel 2: %i\n",
                  prediction_type_b);
        }
      } else { /* not compressed, easy case */
        if (alac->setinfo_sample_size <= 16) {
          int i;
          for (i = 0; i < outputsamples; i++) {
            int32_t audiobits_a, audiobits_b;

            audiobits_a = readbits(alac, alac->setinfo_sample_size);
            audiobits_b = readbits(alac, alac->setinfo_sample_size);

            audiobits_a = SIGN_EXTENDED32(audiobits_a, alac->setinfo_sample_size);
            audiobits_b = SIGN_EXTENDED32(audiobits_b, alac->setinfo_sample_size);

            alac->outputsamples_buffer_a[i] = audiobits_a;
            alac->outputsamples_buffer_b[i] = audiobits_b;
          }
        } else {
          int i;
          for (i = 0; i < outputsamples; i++) {
            int32_t audiobits_a, audiobits_b;

            audiobits_a = readbits(alac, 16);
            audiobits_a = audiobits_a << (alac->setinfo_sample_size - 16);
            audiobits_a |= readbits(alac, alac->setinfo_sample_size - 16);
            audiobits_a = SignExtend24(audiobits_a);

            audiobits_b = readbits(alac, 16);
            audiobits_b = audiobits_b << (alac->setinfo_sample_size - 16);
            audiobits_b |= readbits(alac, alac->setinfo_sample_size - 16);
            audiobits_b = SignExtend24(audiobits_b);

            alac->outputsamples_buffer_a[i] = audiobits_a;
            alac->outputsamples_buffer_b[i] = audiobits_b;
          }
        }
        uncompressed_bytes = 0; // always 0 for uncompressed
        interlacing_shift = 0;
        interlacing_leftweight = 0;
      }

      switch (alac->setinfo_sample_size) {
        case 16: {
          deinterlace_16(alac->outputsamples_buffer_a, alac->outputsamples_buffer_b,
                         (int16_t *)outbuffer, alac->numchannels, outputsamples, interlacing_shift,
                         interlacing_leftweight);
          break;
        }
        case 24: {
          deinterlace_24(alac->outputsamples_buffer_a, alac->outputsamples_buffer_b,
                         uncompressed_bytes, alac->uncompressed_bytes_buffer_a,
                         alac->uncompressed_bytes_buffer_b, (int16_t *)outbuffer,
                         alac->numchannels, outputsamples, interlacing_shift,
                         interlacing_leftweight);
          break;
        }
        case 20:
        case 32:
          fprintf(stderr, "FIXME: unimplemented sample size %i\n", alac->setinfo_sample_size);
          break;
        default:
          break;
      }

      break;
    }
  }
}

/* supports reading 1 to 32 bits, in big endian format */
static uint32_t readbits(alac_file *alac, int bits) {
  int32_t result = 0;

  if (bits > 16) {
    bits -= 16;
    result = readbits_16(alac, 16) << bits;
  }

  result |= readbits_16(alac, bits);

  return result;
}

/* reads a single bit */
static int readbit(alac_file *alac) {
  int result;
  int new_accumulator;

  result = alac->input_buffer[0];

  result = result << alac->input_buffer_bitaccumulator;

  result = result >> 7 & 1;

  new_accumulator = (alac->input_buffer_bitaccumulator + 1);

  alac->input_buffer += (new_accumulator / 8);

  alac->input_buffer_bitaccumulator = (new_accumulator % 8);

  return result;
}

static void entropy_rice_decode(alac_file *alac, int32_t *outputBuffer, int outputSize,
                                int readSampleSize, int rice_initialhistory, int rice_kmodifier,
                                int rice_historymult, int rice_kmodifier_mask) {
  int outputCount;
  int history = rice_initialhistory;
  int signModifier = 0;

  for (outputCount = 0; outputCount < outputSize; outputCount++) {
    int32_t decodedValue;
    int32_t finalValue;
    int32_t k;

    k = 31 - rice_kmodifier - count_leading_zeros((history >> 9) + 3);

    if (k < 0)
      k += rice_kmodifier;
    else
      k = rice_kmodifier;

    // note: don't use rice_kmodifier_mask here (set mask to 0xFFFFFFFF)
    decodedValue = entropy_decode_value(alac, readSampleSize, k, 0xFFFFFFFF);

    decodedValue += signModifier;
    finalValue = (decodedValue + 1) / 2; // inc by 1 and shift out sign bit
    if (decodedValue & 1)                // the sign is stored in the low bit
      finalValue *= -1;

    outputBuffer[outputCount] = finalValue;

    signModifier = 0;

    // update history
    history += (decodedValue * rice_historymult) - ((history * rice_historymult) >> 9);

    if (decodedValue > 0xFFFF)
      history = 0xFFFF;

    // special case, for compressed blocks of 0
    if ((history < 128) && (outputCount + 1 < outputSize)) {
      int32_t blockSize;

      signModifier = 1;

      k = count_leading_zeros(history) + ((history + 16) / 64) - 24;

      // note: blockSize is always 16bit
      blockSize = entropy_decode_value(alac, 16, k, rice_kmodifier_mask);

      // got blockSize 0s
      if (blockSize > 0) {
        memset(&outputBuffer[outputCount + 1], 0, blockSize * sizeof(*outputBuffer));
        outputCount += blockSize;
      }

      if (blockSize > 0xFFFF)
        signModifier = 0;

      history = 0;
    }
  }
}

static void predictor_decompress_fir_adapt(int32_t *error_buffer, int32_t *buffer_out,
                                           int output_size, int readsamplesize,
                                           int16_t *predictor_coef_table, int predictor_coef_num,
                                           int predictor_quantitization) {
  int i;

  /* first sample always copies */
  *buffer_out = *error_buffer;

  if (!predictor_coef_num) {
    if (output_size <= 1)
      return;
    memcpy(buffer_out + 1, error_buffer + 1, (output_size - 1) * 4);
    return;
  }

  if (predictor_coef_num == 0x1f) /* 11111 - max value of predictor_coef_num */
  {                               /* second-best case scenario for fir decompression,
                                   * error describes a small difference from the previous sample only
                                   */
    if (output_size <= 1)
      return;
    for (i = 0; i < output_size - 1; i++) {
      int32_t prev_value;
      int32_t error_value;

      prev_value = buffer_out[i];
      error_value = error_buffer[i + 1];
      buffer_out[i + 1] = SIGN_EXTENDED32((prev_value + error_value), readsamplesize);
    }
    return;
  }

  /* read warm-up samples */
  if (predictor_coef_num > 0) {
    int i;
    for (i = 0; i < predictor_coef_num; i++) {
      int32_t val;

      val = buffer_out[i] + error_buffer[i + 1];

      val = SIGN_EXTENDED32(val, readsamplesize);

      buffer_out[i + 1] = val;
    }
  }

#if 0
    /* 4 and 8 are very common cases (the only ones i've seen). these
     * should be unrolled and optimised
     */
    if (predictor_coef_num == 4)
    {
        /* FIXME: optimised general case */
        return;
    }

    if (predictor_coef_table == 8)
    {
        /* FIXME: optimised general case */
        return;
    }
#endif

  /* general case */
  if (predictor_coef_num > 0) {
    for (i = predictor_coef_num + 1; i < output_size; i++) {
      int j;
      int sum = 0;
      int outval;
      int error_val = error_buffer[i];

      for (j = 0; j < predictor_coef_num; j++) {
        sum += (buffer_out[predictor_coef_num - j] - buffer_out[0]) * predictor_coef_table[j];
      }

      outval = (1 << (predictor_quantitization - 1)) + sum;
      outval = outval >> predictor_quantitization;
      outval = outval + buffer_out[0] + error_val;
      outval = SIGN_EXTENDED32(outval, readsamplesize);

      buffer_out[predictor_coef_num + 1] = outval;

      if (error_val > 0) {
        int predictor_num = predictor_coef_num - 1;

        while (predictor_num >= 0 && error_val > 0) {
          int val = buffer_out[0] - buffer_out[predictor_coef_num - predictor_num];
          int sign = SIGN_ONLY(val);

          predictor_coef_table[predictor_num] -= sign;

          val *= sign; /* absolute value */

          error_val -= ((val >> predictor_quantitization) * (predictor_coef_num - predictor_num));

          predictor_num--;
        }
      } else if (error_val < 0) {
        int predictor_num = predictor_coef_num - 1;

        while (predictor_num >= 0 && error_val < 0) {
          int val = buffer_out[0] - buffer_out[predictor_coef_num - predictor_num];
          int sign = -SIGN_ONLY(val);

          predictor_coef_table[predictor_num] -= sign;

          val *= sign; /* neg value */

          error_val -= ((val >> predictor_quantitization) * (predictor_coef_num - predictor_num));

          predictor_num--;
        }
      }

      buffer_out++;
    }
  }
}

static void deinterlace_16(int32_t *buffer_a, int32_t *buffer_b, int16_t *buffer_out,
                           int numchannels, int numsamples, uint8_t interlacing_shift,
                           uint8_t interlacing_leftweight) {
  int i;
  if (numsamples <= 0)
    return;

  /* weighted interlacing */
  if (interlacing_leftweight) {
    for (i = 0; i < numsamples; i++) {
      int32_t difference, midright;
      int16_t left;
      int16_t right;

      midright = buffer_a[i];
      difference = buffer_b[i];

      right = midright - ((difference * interlacing_leftweight) >> interlacing_shift);
      left = right + difference;

      /* output is always little endian */
      if (host_bigendian) {
        _Swap16(left);
        _Swap16(right);
      }

      buffer_out[i * numchannels] = left;
      buffer_out[i * numchannels + 1] = right;
    }

    return;
  }

  /* otherwise basic interlacing took place */
  for (i = 0; i < numsamples; i++) {
    int16_t left, right;

    left = buffer_a[i];
    right = buffer_b[i];

    /* output is always little endian */
    if (host_bigendian) {
      _Swap16(left);
      _Swap16(right);
    }

    buffer_out[i * numchannels] = left;
    buffer_out[i * numchannels + 1] = right;
  }
}

static void deinterlace_24(int32_t *buffer_a, int32_t *buffer_b, int uncompressed_bytes,
                           int32_t *uncompressed_bytes_buffer_a,
                           int32_t *uncompressed_bytes_buffer_b, void *buffer_out, int numchannels,
                           int numsamples, uint8_t interlacing_shift,
                           uint8_t interlacing_leftweight) {
  int i;
  if (numsamples <= 0)
    return;

  /* weighted interlacing */
  if (interlacing_leftweight) {
    for (i = 0; i < numsamples; i++) {
      int32_t difference, midright;
      int32_t left;
      int32_t right;

      midright = buffer_a[i];
      difference = buffer_b[i];

      right = midright - ((difference * interlacing_leftweight) >> interlacing_shift);
      left = right + difference;

      if (uncompressed_bytes) {
        uint32_t mask = ~(0xFFFFFFFF << (uncompressed_bytes * 8));
        left <<= (uncompressed_bytes * 8);
        right <<= (uncompressed_bytes * 8);

        left |= uncompressed_bytes_buffer_a[i] & mask;
        right |= uncompressed_bytes_buffer_b[i] & mask;
      }

      ((uint8_t *)buffer_out)[i * numchannels * 3] = (left)&0xFF;
      ((uint8_t *)buffer_out)[i * numchannels * 3 + 1] = (left >> 8) & 0xFF;
      ((uint8_t *)buffer_out)[i * numchannels * 3 + 2] = (left >> 16) & 0xFF;

      ((uint8_t *)buffer_out)[i * numchannels * 3 + 3] = (right)&0xFF;
      ((uint8_t *)buffer_out)[i * numchannels * 3 + 4] = (right >> 8) & 0xFF;
      ((uint8_t *)buffer_out)[i * numchannels * 3 + 5] = (right >> 16) & 0xFF;
    }

    return;
  }

  /* otherwise basic interlacing took place */
  for (i = 0; i < numsamples; i++) {
    int32_t left, right;

    left = buffer_a[i];
    right = buffer_b[i];

    if (uncompressed_bytes) {
      uint32_t mask = ~(0xFFFFFFFF << (uncompressed_bytes * 8));
      left <<= (uncompressed_bytes * 8);
      right <<= (uncompressed_bytes * 8);

      left |= uncompressed_bytes_buffer_a[i] & mask;
      right |= uncompressed_bytes_buffer_b[i] & mask;
    }

    ((uint8_t *)buffer_out)[i * numchannels * 3] = (left)&0xFF;
    ((uint8_t *)buffer_out)[i * numchannels * 3 + 1] = (left >> 8) & 0xFF;
    ((uint8_t *)buffer_out)[i * numchannels * 3 + 2] = (left >> 16) & 0xFF;

    ((uint8_t *)buffer_out)[i * numchannels * 3 + 3] = (right)&0xFF;
    ((uint8_t *)buffer_out)[i * numchannels * 3 + 4] = (right >> 8) & 0xFF;
    ((uint8_t *)buffer_out)[i * numchannels * 3 + 5] = (right >> 16) & 0xFF;
  }
}

static int count_leading_zeros(int input) {
  int output = 0;
  int curbyte = 0;

  curbyte = input >> 24;
  if (curbyte)
    goto found;
  output += 8;

  curbyte = input >> 16;
  if (curbyte & 0xff)
    goto found;
  output += 8;

  curbyte = input >> 8;
  if (curbyte & 0xff)
    goto found;
  output += 8;

  curbyte = input;
  if (curbyte & 0xff)
    goto found;
  output += 8;

  return output;

found:
  if (!(curbyte & 0xf0)) {
    output += 4;
  } else
    curbyte >>= 4;

  if (curbyte & 0x8)
    return output;
  if (curbyte & 0x4)
    return output + 1;
  if (curbyte & 0x2)
    return output + 2;
  if (curbyte & 0x1)
    return output + 3;

  /* shouldn't get here: */
  return output + 4;
}

#define RICE_THRESHOLD 8 // maximum number of bits for a rice prefix.

static int32_t entropy_decode_value(alac_file *alac, int readSampleSize, int k,
                                    int rice_kmodifier_mask) {
  int32_t x = 0; // decoded value

  // read x, number of 1s before 0 represent the rice value.
  while (x <= RICE_THRESHOLD && readbit(alac)) {
    x++;
  }

  if (x > RICE_THRESHOLD) {
    // read the number from the bit stream (raw value)
    int32_t value;

    value = readbits(alac, readSampleSize);

    // mask value
    value &= (((uint32_t)0xffffffff) >> (32 - readSampleSize));

    x = value;
  } else {
    if (k != 1) {
      int extraBits = readbits(alac, k);

      // x = x * (2^k - 1)
      x *= (((1 << k) - 1) & rice_kmodifier_mask);

      if (extraBits > 1)
        x += extraBits - 1;
      else
        unreadbits(alac, 1);
    }
  }

  return x;
}

void *rtp_buffered_audio_processor(void *arg) {
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;

  pthread_t *buffered_reader_thread = malloc(sizeof(pthread_t));

  // initialise the buffer data structure
  buffered_audio->buffer_max_size = conn->ap2_audio_buffer_size;

  // pthread_mutex_lock(&conn->buffered_audio_mutex);
  buffered_audio->toq = buffered_audio->buffer;
  buffered_audio->eoq = buffered_audio->buffer;

  buffered_audio->sock_fd = conn->buffered_audio_socket;

  pthread_create(buffered_reader_thread, NULL, &buffered_tcp_reader, buffered_audio);

  // ideas and some code from
  // https://rodic.fr/blog/libavcodec-tutorial-decode-audio-file/ with thanks

  // initialize all muxers, demuxers and protocols for libavformat
  // (does nothing if called twice during the course of one program execution)
  // deprecated in ffmpeg 4.0 and later... but still needed in ffmpeg 3.6 /
  // ubuntu 18

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  avcodec_register_all();
#pragma GCC diagnostic pop

  AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
  if (codec == NULL) {
    debug(1, "Can't find an AAC decoder!");
  }

  AVCodecContext *codec_context = avcodec_alloc_context3(codec);
  if (codec_context == NULL) {
    debug(1, "Could not allocate audio codec context!");
  }
  // push a deallocator -- av_free(codec_context)
  pthread_cleanup_push(avcodec_alloc_context3_cleanup_handler, codec_context);

  if (avcodec_open2(codec_context, codec, NULL) < 0) {
    debug(1, "Could not open a codec into the audio codec context");
  }
  // push a closer -- avcodec_close(codec_context);
  pthread_cleanup_push(avcodec_open2_cleanup_handler, codec_context);

  AVCodecParserContext *codec_parser_context = av_parser_init(codec->id);
  if (codec_parser_context == NULL) {
    debug(1, "Can't initialise a parser context!");
  }
  // push a closer -- av_parser_close(codec_parser_context);
  pthread_cleanup_push(av_parser_init_cleanup_handler, codec_parser_context);

  AVPacket *pkt = av_packet_alloc();
  if (pkt == NULL) {
    debug(1, "Can't allocate an AV packet");
  }
  // push a deallocator -- av_packet_free(pkt);
  pthread_cleanup_push(av_packet_alloc_cleanup_handler, &pkt);

  AVFrame *decoded_frame = NULL;
  int dst_linesize;
  int dst_bufsize;

  // Prepare software resampler to convert floating point (?)
  SwrContext *swr = swr_alloc();
  if (swr == NULL) {
    debug(1, "can not allocate a swr context");
  }
  // push a deallocator -- av_packet_free(pkt);
  pthread_cleanup_push(swr_alloc_cleanup_handler, &swr);

  av_opt_set_int(swr, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
  av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
  av_opt_set_int(swr, "in_sample_rate", conn->input_rate, 0);
  av_opt_set_int(swr, "out_sample_rate", conn->input_rate,
                 0); // must match or the timing will be wrong`
  av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

  enum AVSampleFormat av_format;
  switch (config.output_format) {
    case SPS_FORMAT_S32:
    case SPS_FORMAT_S32_LE:
    case SPS_FORMAT_S32_BE:
    case SPS_FORMAT_S24:
    case SPS_FORMAT_S24_LE:
    case SPS_FORMAT_S24_BE:
    case SPS_FORMAT_S24_3LE:
    case SPS_FORMAT_S24_3BE:
      av_format = AV_SAMPLE_FMT_S32;
      conn->input_bytes_per_frame = 8; // the output from the decoder will be input to the player
      conn->input_bit_depth = 32;
      debug(2, "32-bit output format chosen");
      break;
    case SPS_FORMAT_S16:
    case SPS_FORMAT_S16_LE:
    case SPS_FORMAT_S16_BE:
      av_format = AV_SAMPLE_FMT_S16;
      conn->input_bytes_per_frame = 4;
      conn->input_bit_depth = 16;
      break;
    case SPS_FORMAT_U8:
      av_format = AV_SAMPLE_FMT_U8;
      conn->input_bytes_per_frame = 2;
      conn->input_bit_depth = 8;
      break;
    default:
      debug(1,
            "Unsupported DAC output format %u. AV_SAMPLE_FMT_S16 decoding "
            "chosen. Good luck!",
            config.output_format);
      av_format = AV_SAMPLE_FMT_S16;
      conn->input_bytes_per_frame = 4; // the output from the decoder will be input to the player
      conn->input_bit_depth = 16;
      break;
  };

  av_opt_set_sample_fmt(swr, "out_sample_fmt", av_format, 0);
  swr_init(swr);

  uint8_t packet[16 * 1024];
  unsigned char m[16 * 1024]; // leave the first 7 bytes blank to make room for the ADTS
  uint8_t *pcm_audio = NULL;  // the S16 output
  unsigned char *data_to_process;
  ssize_t data_remaining;
  uint32_t seq_no; // audio packet number
  // uint32_t previous_seq_no = 0;
  int new_buffer_needed = 0;
  ssize_t nread;

  int finished = 0;
  int pcm_buffer_size = (1024 + 352) * conn->input_bytes_per_frame;
  uint8_t pcm_buffer[pcm_buffer_size];

  int pcm_buffer_occupancy = 0;
  int pcm_buffer_read_point = 0; // offset to where the next buffer should come from
  uint32_t pcm_buffer_read_point_rtptime = 0;
  // uint32_t expected_rtptime;

  uint64_t blocks_read = 0;
  uint64_t blocks_read_since_flush = 0;
  int flush_requested = 0;

  uint32_t timestamp;
  int streaming_has_started = 0;
  int play_enabled = 0;
  uint32_t flush_from_timestamp;
  double requested_lead_time = 0.1; // normal lead time minimum -- maybe  it should be about 0.1

  // wait until our timing information is valid

  // debug(1,"rtp_buffered_audio_processor ready.");
  while (have_ptp_timing_information(conn) == 0)
    usleep(1000);

  reset_buffer(conn); // in case there is any garbage in the player
  // int not_first_time_out = 0;

  // quick check of parameters
  if (conn->input_bytes_per_frame == 0)
    die("conn->input_bytes_per_frame is zero!");
  do {
    int flush_is_delayed = 0;
    int flush_newly_requested = 0;
    int flush_newly_complete = 0;
    int play_newly_stopped = 0;
    // are we in in flush mode, or just about to leave it?
    debug_mutex_lock(&conn->flush_mutex, 25000,
                     1); // 25 ms is a long time to wait!
    uint32_t flushUntilSeq = conn->ap2_flush_until_sequence_number;
    uint32_t flushUntilTS = conn->ap2_flush_until_rtp_timestamp;

    int flush_request_active = 0;
    if (conn->ap2_flush_requested) {
      if (conn->ap2_flush_from_valid == 0) { // i.e. a flush from right now
        flush_request_active = 1;
        flush_is_delayed = 0;
      } else {
        flush_is_delayed = 1;
        flush_from_timestamp = conn->ap2_flush_from_rtp_timestamp;
        int32_t blocks_to_start_of_flush = conn->ap2_flush_from_sequence_number - seq_no;
        if (blocks_to_start_of_flush <= 0) {
          flush_request_active = 1;
        }
      }
    }
    // if we are in flush mode
    if (flush_request_active) {
      if (flush_requested == 0) {
        // here, a flush has been newly requested

        debug(2, "Flush requested.");
        if (conn->ap2_flush_from_valid) {
          debug(2, "  fromTS:          %u", conn->ap2_flush_from_rtp_timestamp);
          debug(2, "  fromSeq:         %u", conn->ap2_flush_from_sequence_number);
          debug(2, "--");
        }
        debug(2, "  untilTS:         %u", conn->ap2_flush_until_rtp_timestamp);
        debug(2, "  untilSeq:        %u", conn->ap2_flush_until_sequence_number);
        debug(2, "--");
        debug(2, "  currentTS_Start: %u", pcm_buffer_read_point_rtptime);
        uint32_t fib = (pcm_buffer_occupancy - pcm_buffer_read_point) / 4;
        debug(2, "  framesInBuffer:  %u", fib);
        uint32_t endTS = fib + pcm_buffer_read_point_rtptime;
        debug(2, "  currentTS_End:   %u", endTS); // a frame occupies 4 bytes
        debug(2, "  currentSeq:      %u", seq_no);

        flush_newly_requested = 1;
      }
      // blocks_read to ensure seq_no is valid
      if ((blocks_read != 0) && (seq_no >= flushUntilSeq)) {
        // we have reached or overshot the flushUntilSeq block
        if (flushUntilSeq != seq_no)
          debug(2,
                "flush request ended with flushUntilSeq %u overshot at %u, "
                "flushUntilTS: %u, "
                "incoming timestamp: %u.",
                flushUntilSeq, seq_no, flushUntilTS, timestamp);
        else
          debug(2,
                "flush request ended with flushUntilSeq, flushUntilTS: %u, "
                "incoming timestamp: %u",
                flushUntilSeq, flushUntilTS, timestamp);
        conn->ap2_flush_requested = 0;
        flush_request_active = 0;
        flush_newly_requested = 0;
      }
    }
    if ((flush_requested) && (flush_request_active == 0))
      flush_newly_complete = 1;
    flush_requested = flush_request_active;
    // flush_requested = conn->ap2_flush_requested;
    if ((play_enabled) && (conn->ap2_play_enabled == 0))
      play_newly_stopped = 1;
    play_enabled = conn->ap2_play_enabled;
    debug_mutex_unlock(&conn->flush_mutex, 3);

    // do this outside the flush mutex
    if (flush_newly_complete) {
      debug(2, "Flush Complete.");
      blocks_read_since_flush = 0;
    }

    if (play_newly_stopped != 0)
      reset_buffer(conn); // stop play ASAP

    if (flush_newly_requested) {
      reset_buffer(conn);

      if (flush_is_delayed == 0) {
        debug(2, "Immediate Buffered Audio Flush Started.");
        // player_full_flush(conn);
        streaming_has_started = 0;
        pcm_buffer_occupancy = 0;
        pcm_buffer_read_point = 0;
      } else {
        debug(2, "Delayed Buffered Audio Flush Started.");
        streaming_has_started = 0;
        pcm_buffer_occupancy = 0;
        pcm_buffer_read_point = 0;
      }
    }

    // now, if a flush is not requested, we can do the normal stuff
    if (flush_requested == 0) {
      // is there space in the player thread's buffer system?
      unsigned int player_buffer_size, player_buffer_occupancy;
      get_audio_buffer_size_and_occupancy(&player_buffer_size, &player_buffer_occupancy, conn);
      // debug(1,"player buffer size and occupancy: %u and %u",
      // player_buffer_size, player_buffer_occupancy);
      if (player_buffer_occupancy > ((requested_lead_time + 0.4) * conn->input_rate /
                                     352)) { // must be greater than the lead time.
        // if there is enough stuff in the player's buffer, sleep for a while
        // and try again
        usleep(1000); // wait for a while
      } else {
        if ((pcm_buffer_occupancy - pcm_buffer_read_point) >=
            (352 * conn->input_bytes_per_frame)) {
          new_buffer_needed = 0;
          // send a frame to the player if allowed
          // it it's way too late, it probably means that a new anchor time is
          // needed

          /*
                    uint32_t at_rtp = conn->reference_timestamp;
                    at_rtp =
                        at_rtp - (44100 * 10); // allow it to start a few
             seconds late, but not madly late int rtp_diff =
             pcm_buffer_read_point_rtptime - at_rtp;
          */

          if ((play_enabled) && (have_ptp_timing_information(conn) != 0)) {
            uint64_t buffer_should_be_time;
            if (frame_to_local_time(pcm_buffer_read_point_rtptime, &buffer_should_be_time, conn) ==
                0) {
              int64_t lead_time = buffer_should_be_time - get_absolute_time_in_ns();

              // it seems that some garbage blocks can be left after the flush,
              // so only accept them if they have sensible lead times
              if ((lead_time < (int64_t)5000000000L) && (lead_time >= 0)) {
                // if it's the very first block (thus no priming needed)
                if ((blocks_read == 1) || (blocks_read_since_flush > 3)) {
                  if ((lead_time >= (int64_t)(requested_lead_time * 1000000000L)) ||
                      (streaming_has_started != 0)) {
                    if (streaming_has_started == 0)
                      debug(2,
                            "Connection %d: buffered audio starting frame: %u, "
                            "lead time: %f "
                            "seconds.",
                            conn->connection_number, pcm_buffer_read_point_rtptime,
                            0.000000001 * lead_time);
                    // else {
                    // if (expected_rtptime != pcm_buffer_read_point_rtptime)
                    //  debug(1,"actual rtptime is %u, expected rtptime is %u.",
                    //  pcm_buffer_read_point_rtptime, expected_rtptime);
                    //}
                    // expected_rtptime = pcm_buffer_read_point_rtptime + 352;

                    // this is a diagnostic for introducing a timing error that
                    // will force the processing chain to resync
                    // clang-format off
                  /*
                  if ((not_first_time_out == 0) && (blocks_read >= 20)) {
                    int timing_error = 150;
                    debug(1, "Connection %d: Introduce a timing error of %d milliseconds.",
                          conn->connection_number, timing_error);
                    if (timing_error >= 0)
                      pcm_buffer_read_point_rtptime += (conn->input_rate * timing_error) / 1000;
                    else
                      pcm_buffer_read_point_rtptime -= (conn->input_rate * (-timing_error)) / 1000;
                    not_first_time_out = 1;
                  }
                  */
                    // clang-format on

                    player_put_packet(0, 0, pcm_buffer_read_point_rtptime,
                                      pcm_buffer + pcm_buffer_read_point, 352, conn);
                    streaming_has_started++;
                  }
                }
              } else {
                debug(2,
                      "Dropping packet %u from block %u with out-of-range "
                      "lead_time: %.3f seconds.",
                      pcm_buffer_read_point_rtptime, seq_no, 0.000000001 * lead_time);
              }

              pcm_buffer_read_point_rtptime += 352;
              pcm_buffer_read_point += 352 * conn->input_bytes_per_frame;
            } else {
              debug(1, "frame to local time error");
            }
          } else {
            usleep(1000); // wait before asking if play is enabled again
          }
        } else {
          new_buffer_needed = 1;
          if (pcm_buffer_read_point != 0) {
            // debug(1,"pcm_buffer_read_point (frames): %u, pcm_buffer_occupancy
            // (frames): %u", pcm_buffer_read_point/conn->input_bytes_per_frame,
            // pcm_buffer_occupancy/conn->input_bytes_per_frame); if there is
            // anything to move down to the front of the buffer, do it now;
            if ((pcm_buffer_occupancy - pcm_buffer_read_point) > 0) {
              // move the remaining frames down to the start of the buffer
              // debug(1,"move the remaining frames down to the start of the
              // pcm_buffer");
              memcpy(pcm_buffer, pcm_buffer + pcm_buffer_read_point,
                     pcm_buffer_occupancy - pcm_buffer_read_point);
              pcm_buffer_occupancy = pcm_buffer_occupancy - pcm_buffer_read_point;
            } else {
              // debug(1,"nothing to move to the front of the buffer");
              pcm_buffer_occupancy = 0;
            }
            pcm_buffer_read_point = 0;
          }
        }
      }
    }
    if ((flush_requested) || (new_buffer_needed)) {
      // debug(1,"pcm_buffer_read_point (frames): %u, pcm_buffer_occupancy
      // (frames): %u", pcm_buffer_read_point/4, pcm_buffer_occupancy/4); ok, so
      // here we know we need material from the sender do we will get in a
      // packet of audio
      uint16_t data_len;
      // here we read from the buffer that our thread has been reading
      size_t bytes_remaining_in_buffer;
      nread = lread_sized_block(buffered_audio, &data_len, sizeof(data_len),
                                &bytes_remaining_in_buffer);
      if ((conn->ap2_audio_buffer_minimum_size < 0) ||
          (bytes_remaining_in_buffer < (size_t)conn->ap2_audio_buffer_minimum_size))
        conn->ap2_audio_buffer_minimum_size = bytes_remaining_in_buffer;
      if (nread < 0) {
        char errorstring[1024];
        strerror_r(errno, (char *)errorstring, sizeof(errorstring));
        debug(1,
              "error in rtp_buffered_audio_processor %d: \"%s\". Could not "
              "recv a data_len .",
              errno, errorstring);
      }
      data_len = ntohs(data_len);
      // debug(1,"buffered audio packet of size %u detected.", data_len - 2);
      nread = lread_sized_block(buffered_audio, packet, data_len - 2, &bytes_remaining_in_buffer);
      if ((conn->ap2_audio_buffer_minimum_size < 0) ||
          (bytes_remaining_in_buffer < (size_t)conn->ap2_audio_buffer_minimum_size))
        conn->ap2_audio_buffer_minimum_size = bytes_remaining_in_buffer;
      // debug(1, "buffered audio packet of size %u received.", nread);
      if (nread < 0) {
        char errorstring[1024];
        strerror_r(errno, (char *)errorstring, sizeof(errorstring));
        debug(1,
              "error in rtp_buffered_audio_processor %d: \"%s\". Could not "
              "recv a data packet.",
              errno, errorstring);
      } else if (nread > 0) {
        blocks_read++; // note, this doesn't mean they are valid audio blocks
        blocks_read_since_flush++;
        // debug(1, "Realtime Audio Receiver Packet of length %d received.",
        // nread); now get hold of its various bits and pieces
        /*
        uint8_t version = (packet[0] & 0b11000000) >> 6;
        uint8_t padding = (packet[0] & 0b00100000) >> 5;
        uint8_t extension = (packet[0] & 0b00010000) >> 4;
        uint8_t csrc_count = packet[0] & 0b00001111;
        */
        seq_no = packet[1] * (1 << 16) + packet[2] * (1 << 8) + packet[3];
        timestamp = nctohl(&packet[4]);
        // debug(1, "immediately: block %u, rtptime %u", seq_no, timestamp);
        // uint32_t ssrc = nctohl(&packet[8]);
        // uint8_t marker = 0;
        // uint8_t payload_type = 0;

        // previous_seq_no = seq_no;

        // at this point, we can check if we can to flush this packet -- we
        // won't have to decipher it first debug(1,"seq_no %u, timestamp %u",
        // seq_no, timestamp);

        uint64_t local_should_be_time = 0;
        int have_time_information = frame_to_local_time(timestamp, &local_should_be_time, conn);
        int64_t local_lead_time = 0;
        int64_t requested_lead_time_ns = (int64_t)(requested_lead_time * 1000000000);
        // requested_lead_time_ns = (int64_t)(-300000000);
        // debug(1,"requested_lead_time_ns is actually %f milliseconds.",
        // requested_lead_time_ns * 1E-6);
        int outdated = 0;
        if (have_time_information == 0) {
          local_lead_time = local_should_be_time - get_absolute_time_in_ns();
          // debug(1,"local_lead_time is actually %f milliseconds.",
          // local_lead_time * 1E-6);
          outdated = (local_lead_time < requested_lead_time_ns);
          // if (outdated != 0)
          // debug(1,"Frame is outdated %d if lead_time %" PRId64 " is less than
          // requested lead time
          // %" PRId64 " ns.", outdated, local_lead_time,
          // requested_lead_time_ns);
        } else {
          debug(1, "Timing information not valid");
          // outdated = 1;
        }

        if ((flush_requested) && (seq_no >= flushUntilSeq)) {
          if ((have_time_information == 0) && (play_enabled)) {
            // play enabled will be off when this is a full flush and the anchor
            // information is not valid
            debug(2,
                  "flush completed to seq: %u, flushUntilTS; %u with rtptime: "
                  "%u, lead time: "
                  "0x%" PRIx64 " nanoseconds, i.e. %f sec.",
                  seq_no, flushUntilTS, timestamp, local_lead_time, local_lead_time * 0.000000001);
          } else {
            debug(2, "flush completed to seq: %u with rtptime: %u.", seq_no, timestamp);
          }
        }

        // if we are here because of a flush request, it must be the case that
        // flushing the pcm buffer wasn't enough, as the request would have been
        // turned off by now so we better indicate that the pcm buffer is empty
        // and its contents invalid

        // also, if the incomgin frame is outdated, set pcm_buffer_occupancy to
        // 0;
        if ((flush_requested) || (outdated)) {
          pcm_buffer_occupancy = 0;
        }

        // decode the block and add it to or put it in the pcm buffer

        if (pcm_buffer_occupancy == 0) {
          // they should match and the read point should be zero
          // if ((blocks_read != 0) && (pcm_buffer_read_point_rtptime !=
          // timestamp)) {
          //  debug(2, "set pcm_buffer_read_point_rtptime from %u to %u.",
          //        pcm_buffer_read_point_rtptime, timestamp);
          pcm_buffer_read_point_rtptime = timestamp;
          pcm_buffer_read_point = 0;
          //}
        }

        if (((flush_requested != 0) && (seq_no == flushUntilSeq)) ||
            ((flush_requested == 0) && (new_buffer_needed))) {
          unsigned char nonce[12];
          memset(nonce, 0, sizeof(nonce));
          memcpy(nonce + 4, packet + nread - 8,
                 8); // front-pad the 8-byte nonce received to get the 12-byte
                     // nonce expected

          // https://libsodium.gitbook.io/doc/secret-key_cryptography/aead/chacha20-poly1305/ietf_chacha20-poly1305_construction
          // Note: the eight-byte nonce must be front-padded out to 12 bytes.
          unsigned long long new_payload_length = 0;
          int response = crypto_aead_chacha20poly1305_ietf_decrypt(
              m + 7,               // m
              &new_payload_length, // mlen_p
              NULL,                // nsec,
              packet + 12,         // the ciphertext starts 12 bytes in and is followed
                                   // by the MAC tag,
              nread - (8 + 12),    // clen -- the last 8 bytes are the nonce
              packet + 4,          // authenticated additional data
              8,                   // authenticated additional data length
              nonce,
              conn->session_key); // *k
          if (response != 0) {
            debug(1, "Error decrypting audio packet %u -- packet length %d.", seq_no, nread);
          } else {
            // now pass it in to the regular processing chain

            unsigned long long max_int = INT_MAX; // put in the right format
            if (new_payload_length > max_int)
              debug(1, "Madly long payload length!");
            int payload_length = new_payload_length; // change from long long to int
            int aac_packet_length = payload_length + 7;

            // now, fill in the 7-byte ADTS information, which seems to be
            // needed by the decoder we made room for it in the front of the
            // buffer

            addADTStoPacket(m, aac_packet_length);

            // now we are ready to send this to the decoder

            data_to_process = m;
            data_remaining = aac_packet_length;
            int ret = 0;
            // there can be more than one av packet (? terminology) in a block
            int frame_within_block = 0;
            while (data_remaining > 0) {
              if (decoded_frame == NULL) {
                decoded_frame = av_frame_alloc();
                if (decoded_frame == NULL)
                  debug(1, "could not allocate av_frame");
              } else {
                ret = av_parser_parse2(codec_parser_context, codec_context, &pkt->data, &pkt->size,
                                       data_to_process, data_remaining, AV_NOPTS_VALUE,
                                       AV_NOPTS_VALUE, 0);
                if (ret < 0) {
                  debug(1, "error while parsing deciphered audio packet.");
                } else {
                  frame_within_block++;
                  data_to_process += ret;
                  data_remaining -= ret;
                  // debug(1, "frame found");
                  // now pass each packet to be decoded
                  if (pkt->size) {
                    // if (0) {
                    if (pkt->size <= 7) { // no idea about this...
                      debug(2, "malformed AAC packet skipped.");
                    } else {
                      ret = avcodec_send_packet(codec_context, pkt);

                      if (ret < 0) {
                        debug(1,
                              "error sending frame %d of size %d to decoder, "
                              "blocks_read: %u, "
                              "blocks_read_since_flush: %u.",
                              frame_within_block, pkt->size, blocks_read, blocks_read_since_flush);
                      } else {
                        while (ret >= 0) {
                          ret = avcodec_receive_frame(codec_context, decoded_frame);
                          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            break;
                          else if (ret < 0) {
                            debug(1, "error %d during decoding", ret);
                          } else {
                            av_samples_alloc(&pcm_audio, &dst_linesize, codec_context->channels,
                                             decoded_frame->nb_samples, av_format, 1);
                            // remember to free pcm_audio
                            ret = swr_convert(swr, &pcm_audio, decoded_frame->nb_samples,
                                              (const uint8_t **)decoded_frame->extended_data,
                                              decoded_frame->nb_samples);
                            dst_bufsize = av_samples_get_buffer_size(
                                &dst_linesize, codec_context->channels, ret, av_format, 1);
                            // debug(1,"generated %d bytes of PCM",
                            // dst_bufsize); copy the PCM audio into the PCM
                            // buffer. make sure it's big enough first

                            // also, check it if needs to be truncated but to an
                            // impending delayed flush_is_delayed
                            if (flush_is_delayed) {
                              // see if the flush_from_timestamp is in the
                              // buffer
                              int32_t samples_remaining =
                                  (flush_from_timestamp - pcm_buffer_read_point_rtptime);
                              if ((samples_remaining > 0) &&
                                  ((samples_remaining * conn->input_bytes_per_frame) <
                                   dst_bufsize)) {
                                debug(2,
                                      "samples remaining before flush: %d, "
                                      "number of samples %d. "
                                      "flushFromTS: %u, "
                                      "pcm_buffer_read_point_rtptime: %u.",
                                      samples_remaining, dst_bufsize / conn->input_bytes_per_frame,
                                      flush_from_timestamp, pcm_buffer_read_point_rtptime);
                                dst_bufsize = samples_remaining * conn->input_bytes_per_frame;
                              }
                            }
                            if ((pcm_buffer_size - pcm_buffer_occupancy) < dst_bufsize) {
                              debug(1,
                                    "pcm_buffer_read_point (frames): %u, "
                                    "pcm_buffer_occupancy "
                                    "(frames): %u",
                                    pcm_buffer_read_point / conn->input_bytes_per_frame,
                                    pcm_buffer_occupancy / conn->input_bytes_per_frame);
                              pcm_buffer_size = dst_bufsize + pcm_buffer_occupancy;
                              debug(1,
                                    "fatal error! pcm buffer too small at %d "
                                    "bytes.",
                                    pcm_buffer_size);
                            } else {
                              memcpy(pcm_buffer + pcm_buffer_occupancy, pcm_audio, dst_bufsize);
                              pcm_buffer_occupancy += dst_bufsize;
                            }
                            // debug(1,"decoded %d samples",
                            // decoded_frame->nb_samples);
                            // memcpy(sampleBuffer,outputBuffer16,dst_bufsize);
                            av_freep(&pcm_audio);
                          }
                        }
                      }
                    }
                  }
                }
                if (decoded_frame == NULL)
                  debug(1, "decoded_frame is NULL");
                if (decoded_frame != NULL)
                  av_frame_free(&decoded_frame);
              }
            }

            // revert the state of cancellability
          }
        }
      } else {
        // nread is 0 -- the port has been closed
        debug(2, "buffered audio port closed!");
        finished = 1;
      }
    }

  } while (finished == 0);
  debug(2, "Buffered Audio Receiver RTP thread \"normal\" exit.");
  pthread_cleanup_pop(1); // deallocate the swr
  pthread_cleanup_pop(1); // deallocate the av_packet
  pthread_cleanup_pop(1); // av_parser_init_cleanup_handler
  pthread_cleanup_pop(1); // avcodec_open2_cleanup_handler
  pthread_cleanup_pop(1); // avcodec_alloc_context3_cleanup_handler
  pthread_cleanup_pop(1); // thread creation
  pthread_cleanup_pop(1); // buffer malloc
  pthread_cleanup_pop(1); // not_full_cv
  pthread_cleanup_pop(1); // not_empty_cv
  pthread_cleanup_pop(1); // mutex
  pthread_cleanup_pop(1); // descriptor malloc
  pthread_cleanup_pop(1); // pthread_t malloc
  pthread_cleanup_pop(1); // do the cleanup.
  pthread_exit(NULL);
}

void reset_buffer(rtsp_conn_info *conn) {
  debug_mutex_lock(&conn->ab_mutex, 30000, 0);
  ab_resync(conn);
  debug_mutex_unlock(&conn->ab_mutex, 0);
  if (config.output->flush) {
    config.output->flush(); // no cancellation points
                            //            debug(1, "reset_buffer: flush output device.");
  }
}

void get_audio_buffer_size_and_occupancy(unsigned int *size, unsigned int *occupancy,
                                         rtsp_conn_info *conn) {
  debug_mutex_lock(&conn->ab_mutex, 30000, 0);
  *size = BUFFER_FRAMES;
  if (conn->ab_synced) {
    int16_t occ =
        conn->ab_write - conn->ab_read; // will be zero or positive if read and write are within
                                        // 2^15 of each other and write is at or after read
    *occupancy = occ;
  } else {
    *occupancy = 0;
  }
  debug_mutex_unlock(&conn->ab_mutex, 0);
}

int frame_to_local_time(uint32_t timestamp, uint64_t *time, rtsp_conn_info *conn) {
  if (conn->timing_type == ts_ptp)
    return frame_to_ptp_local_time(timestamp, time, conn);
  else
    return frame_to_ntp_local_time(timestamp, time, conn);
}

int local_time_to_frame(uint64_t time, uint32_t *frame, rtsp_conn_info *conn) {
  if (conn->timing_type == ts_ptp)
    return local_ptp_time_to_frame(time, frame, conn);
  else
    return local_ntp_time_to_frame(time, frame, conn);
}

void reset_anchor_info(rtsp_conn_info *conn) {
  if (conn->timing_type == ts_ptp)
    reset_ptp_anchor_info(conn);
  else
    reset_ntp_anchor_info(conn);
}

int have_timestamp_timing_information(rtsp_conn_info *conn) {
  if (conn->timing_type == ts_ptp)
    return have_ptp_timing_information(conn);
  else
    return have_ntp_timestamp_timing_information(conn);
}
// this will read a block of the size specified to the buffer
// and will return either with the block or on error
ssize_t lread_sized_block(buffered_tcp_desc *descriptor, void *buf, size_t count,
                          size_t *bytes_remaining) {
  ssize_t response, nread;
  size_t inbuf = 0; // bytes already in the buffer
  int keep_trying = 1;

  do {
    nread = buffered_read(descriptor, buf + inbuf, count - inbuf, bytes_remaining);
    if (nread == 0) {
      // a blocking read that returns zero means eof -- implies connection
      // closed
      debug(3, "read_sized_block connection closed.");
      keep_trying = 0;
    } else if (nread < 0) {
      if (errno == EAGAIN) {
        debug(1, "read_sized_block getting Error 11 -- EAGAIN from a blocking "
                 "read!");
      }
      if ((errno != ECONNRESET) && (errno != EAGAIN) && (errno != EINTR)) {
        char errorstring[1024];
        strerror_r(errno, (char *)errorstring, sizeof(errorstring));
        debug(1, "read_sized_block read error %d: \"%s\".", errno, (char *)errorstring);
        keep_trying = 0;
      }
    } else {
      inbuf += (size_t)nread;
    }
  } while ((keep_trying != 0) && (inbuf < count));
  if (nread <= 0)
    response = nread;
  else
    response = inbuf;
  return response;
}

// From
// https://stackoverflow.com/questions/18862715/how-to-generate-the-aac-adts-elementary-stream-with-android-mediacodec
// with thanks!
/**
 *  Add ADTS header at the beginning of each and every AAC packet.
 *  This is needed as MediaCodec encoder generates a packet of raw
 *  AAC data.
 *
 *  Note the packetLen must count in the ADTS header itself.
 **/
void addADTStoPacket(uint8_t *packet, int packetLen) {
  int profile = 2; // AAC LC
                   // 39=MediaCodecInfo.CodecProfileLevel.AACObjectELD;
  int freqIdx = 4; // 44.1KHz
  int chanCfg = 2; // CPE

  // fill in ADTS data
  packet[0] = 0xFF;
  packet[1] = 0xF9;
  packet[2] = ((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2);
  packet[3] = ((chanCfg & 3) << 6) + (packetLen >> 11);
  packet[4] = (packetLen & 0x7FF) >> 3;
  packet[5] = ((packetLen & 7) << 5) + 0x1F;
  packet[6] = 0xFC;
}

int frame_to_ptp_local_time(uint32_t timestamp, uint64_t *time, rtsp_conn_info *conn) {
  int result = -1;
  uint32_t anchor_rtptime = 0;
  uint64_t anchor_local_time = 0;
  if (get_ptp_anchor_local_time_info(conn, &anchor_rtptime, &anchor_local_time) == clock_ok) {
    int32_t frame_difference = timestamp - anchor_rtptime;
    int64_t time_difference = frame_difference;
    time_difference = time_difference * 1000000000;
    if (conn->input_rate == 0)
      die("conn->input_rate is zero!");
    time_difference = time_difference / conn->input_rate;
    uint64_t ltime = anchor_local_time + time_difference;
    *time = ltime;
    result = 0;
  } else {
    debug(3, "frame_to_local_time can't get anchor local time information");
  }
  return result;
}

int local_ptp_time_to_frame(uint64_t time, uint32_t *frame, rtsp_conn_info *conn) {
  int result = -1;
  uint32_t anchor_rtptime = 0;
  uint64_t anchor_local_time = 0;
  if (get_ptp_anchor_local_time_info(conn, &anchor_rtptime, &anchor_local_time) == clock_ok) {
    int64_t time_difference = time - anchor_local_time;
    int64_t frame_difference = time_difference;
    frame_difference = frame_difference * conn->input_rate; // but this is by 10^9
    frame_difference = frame_difference / 1000000000;
    int32_t fd32 = frame_difference;
    uint32_t lframe = anchor_rtptime + fd32;
    *frame = lframe;
    result = 0;
  } else {
    debug(3, "local_time_to_frame can't get anchor local time information");
  }
  return result;
}

void reset_ptp_anchor_info(rtsp_conn_info *conn) {
  debug(2, "Connection %d: Clear anchor information.", conn->connection_number);
  conn->last_anchor_info_is_valid = 0;
  conn->anchor_remote_info_is_valid = 0;
}

int have_ptp_timing_information(rtsp_conn_info *conn) {
  if (get_ptp_anchor_local_time_info(conn, NULL, NULL) == clock_ok)
    return 1;
  else
    return 0;
}

ssize_t buffered_read(buffered_tcp_desc *descriptor, void *buf, size_t count,
                      size_t *bytes_remaining) {
  ssize_t response = -1;
  if (pthread_mutex_lock(&descriptor->mutex) != 0)
    debug(1, "problem with mutex");
  pthread_cleanup_push(mutex_unlock, (void *)&descriptor->mutex);
  if (descriptor->closed == 0) {
    if ((descriptor->buffer_occupancy == 0) && (descriptor->error_code == 0)) {
      if (count == 2)
        debug(2, "buffered_read: waiting for %u bytes (okay at start of a track).", count);
      else
        debug(1, "buffered_read: waiting for %u bytes.", count);
    }
    while ((descriptor->buffer_occupancy == 0) && (descriptor->error_code == 0)) {
      if (pthread_cond_wait(&descriptor->not_empty_cv, &descriptor->mutex))
        debug(1, "Error waiting for buffered read");
    }
  }
  if (descriptor->buffer_occupancy != 0) {
    ssize_t bytes_to_move = count;

    if (descriptor->buffer_occupancy < count) {
      bytes_to_move = descriptor->buffer_occupancy;
    }

    ssize_t top_gap = descriptor->buffer + descriptor->buffer_max_size - descriptor->toq;
    if (top_gap < bytes_to_move)
      bytes_to_move = top_gap;

    memcpy(buf, descriptor->toq, bytes_to_move);
    descriptor->toq += bytes_to_move;
    if (descriptor->toq == descriptor->buffer + descriptor->buffer_max_size)
      descriptor->toq = descriptor->buffer;
    descriptor->buffer_occupancy -= bytes_to_move;
    if (bytes_remaining != NULL)
      *bytes_remaining = descriptor->buffer_occupancy;
    response = bytes_to_move;
    if (pthread_cond_signal(&descriptor->not_full_cv))
      debug(1, "Error signalling");
  } else if (descriptor->error_code) {
    errno = descriptor->error_code;
    response = -1;
  } else if (descriptor->closed != 0) {
    response = 0;
  }

  pthread_cleanup_pop(1); // release the mutex
  return response;
}
int long_time_notifcation_done = 0;
int get_ptp_anchor_local_time_info(rtsp_conn_info *conn, uint32_t *anchorRTP,
                                   uint64_t *anchorLocalTime) {
  uint64_t actual_clock_id, actual_time_of_sample, actual_offset, start_of_mastership;
  int response = ptp_get_clock_info(&actual_clock_id, &actual_time_of_sample, &actual_offset,
                                    &start_of_mastership);

  if (response == clock_ok) {
    uint64_t time_now = get_absolute_time_in_ns();
    int64_t time_since_sample = time_now - actual_time_of_sample;
    if (time_since_sample > 300000000000) {
      if (long_time_notifcation_done == 0) {
        debug(1, "The last PTP timing sample is pretty old: %f seconds.",
              0.000000001 * time_since_sample);
        long_time_notifcation_done = 1;
      }
    } else if ((time_since_sample < 2000000000) && (long_time_notifcation_done != 0)) {
      debug(1, "The last PTP timing sample is no longer too old: %f seconds.",
            0.000000001 * time_since_sample);
      long_time_notifcation_done = 0;
    }

    if (conn->anchor_remote_info_is_valid !=
        0) { // i.e. if we have anchor clock ID and anchor time / rtptime
      // figure out how long the clock has been master
      int64_t duration_of_mastership = time_now - start_of_mastership;
      // if we have an alternative, i.e. if last anchor stuff is valid
      // then we can wait for a long time to let the new master settle
      // if we do not, we can wait for some different (shorter) time before
      // using the master clock timing

      if (actual_clock_id == conn->anchor_clock) {
        // if the master clock and the anchor clock are the same
        // wait at least this time before using the new master clock
        // note that mastership may be backdated
        if (duration_of_mastership < 1500000000) {
          debug(3, "master not old enough yet: %f ms", 0.000001 * duration_of_mastership);
          response = clock_not_ready;
        } else if ((duration_of_mastership > 5000000000) ||
                   (conn->last_anchor_info_is_valid == 0)) {
          // use the master clock if it's at least this old or if we have no
          // alternative and it at least is the minimum age.
          conn->last_anchor_rtptime = conn->anchor_rtptime;
          conn->last_anchor_local_time = conn->anchor_time - actual_offset;
          conn->last_anchor_time_of_update = time_now;
          if (conn->last_anchor_info_is_valid == 0)
            conn->last_anchor_validity_start_time = start_of_mastership;
          conn->last_anchor_info_is_valid = 1;
          if (conn->anchor_clock_is_new != 0)
            debug(1,
                  "Connection %d: Clock %" PRIx64
                  " is now the new anchor clock and master clock. History: %f "
                  "milliseconds.",
                  conn->connection_number, conn->anchor_clock, 0.000001 * duration_of_mastership);
          conn->anchor_clock_is_new = 0;
        }
      } else {
        // the anchor clock and the actual clock are different
        // this could happen because
        // the master clock has changed or
        // because the anchor clock has changed

        // so, if the anchor has not changed, it must be that the master clock
        // has changed
        if (conn->anchor_clock_is_new != 0)
          debug(3,
                "Connection %d: Anchor clock has changed to %" PRIx64 ", master clock is: %" PRIx64
                ". History: %f milliseconds.",
                conn->connection_number, conn->anchor_clock, actual_clock_id,
                0.000001 * duration_of_mastership);

        if ((conn->last_anchor_info_is_valid != 0) && (conn->anchor_clock_is_new == 0)) {
          int64_t time_since_last_update =
              get_absolute_time_in_ns() - conn->last_anchor_time_of_update;
          if (time_since_last_update > 5000000000) {
            debug(1,
                  "Connection %d: Master clock has changed to %" PRIx64
                  ". History: %f milliseconds.",
                  conn->connection_number, actual_clock_id, 0.000001 * duration_of_mastership);
            // here we adjust the time of the anchor rtptime
            // we know its local time, so we use the new clocks's offset to
            // calculate what time that must be on the new clock

            // Now, the thing is that while the anchor clock and master clock
            // for a buffered session starts off the same, the master clock can
            // change without the anchor clock changing. SPS allows the new
            // master clock time to settle down and then calculates the
            // appropriate offset to it by calculating back from the local
            // anchor information and the new clock's advertised offset. Of
            // course, small errors will occur. More importantly, the new master
            // clock(s) and the original one will drift at different rates. So,
            // after all this, if the original master clock becomes the master
            // again, then there could be quite a difference in the time
            // information that was calculated through all the clock changes and
            // the actual master clock's time information. What do we do? We can
            // hardly ignore this new and reliable information so we'll take it.
            // Maybe we should add code to slowly correct towards it but at
            // present, we just take it.

            // So, if the master clock has again become equal to the actual
            // anchor clock then we can reinstate it all. first, let us
            // calculate the cumulative offset after swapping all the clocks...
            conn->anchor_time = conn->last_anchor_local_time + actual_offset;

            // we can check how much of a deviation there was going from clock
            // to clock and back around to the master clock

            if (actual_clock_id == conn->actual_anchor_clock) {
              int64_t cumulative_deviation = conn->anchor_time - conn->actual_anchor_time;
              debug(1,
                    "Master clock has become equal to the anchor clock. The "
                    "estimated clock time "
                    "was %f ms ahead(+) or behind (-) the real clock time.",
                    0.000001 * cumulative_deviation);
              conn->anchor_clock = conn->actual_anchor_clock;
              conn->anchor_time = conn->actual_anchor_time;
              conn->anchor_rtptime = conn->actual_anchor_rtptime;
            } else {
              // conn->anchor_time = conn->last_anchor_local_time +
              // actual_offset; // already done
              conn->anchor_clock = actual_clock_id;
            }
            conn->anchor_clock_is_new = 0;
          }
        } else {
          response = clock_not_valid; // no current clock information and no
                                      // previous clock info
        }
      }
    } else {
      // debug(1, "anchor_remote_info_is_valid not valid");
      response = clock_no_anchor_info; // no anchor information
    }
  }

  // here, check and update the clock status
  if ((clock_status_t)response != conn->clock_status) {
    switch (response) {
      case clock_ok:
        debug(1, "Connection %d: NQPTP new master clock %" PRIx64 ".", conn->connection_number,
              actual_clock_id);
        break;
      case clock_not_ready:
        debug(2, "Connection %d: NQPTP master clock %" PRIx64 " is available but not ready.",
              conn->connection_number, actual_clock_id);
        break;
      case clock_service_unavailable:
        debug(1, "Connection %d: NQPTP clock is not available.", conn->connection_number);
        warn("Can't access the NQPTP clock. Is NQPTP running?");
        break;
      case clock_access_error:
        debug(1, "Connection %d: Error accessing the NQPTP clock interface.",
              conn->connection_number);
        break;
      case clock_data_unavailable:
        debug(1, "Connection %d: Can not access NQPTP clock information.",
              conn->connection_number);
        break;
      case clock_no_master:
        debug(2, "Connection %d: No NQPTP master clock.", conn->connection_number);
        break;
      case clock_no_anchor_info:
        debug(1, "Connection %d: No clock anchor information.", conn->connection_number);
        break;
      case clock_version_mismatch:
        debug(1, "Connection %d: NQPTP clock interface mismatch.", conn->connection_number);
        warn("This version of Shairport Sync is not compatible with the "
             "installed version of NQPTP. "
             "Please update.");
        break;
      case clock_not_synchronised:
        debug(1, "Connection %d: NQPTP clock is not synchronised.", conn->connection_number);
        break;
      case clock_not_valid:
        debug(1, "Connection %d: NQPTP clock information is not valid.", conn->connection_number);
        break;
      default:
        debug(1, "Connection %d: NQPTP clock reports an unrecognised status: %u.",
              conn->connection_number, response);
        break;
    }
    conn->clock_status = response;
  }

  if (conn->last_anchor_info_is_valid != 0) {
    if (anchorRTP != NULL)
      *anchorRTP = conn->last_anchor_rtptime;
    if (anchorLocalTime != NULL)
      *anchorLocalTime = conn->last_anchor_local_time;
  }

  return response;
}

void handle_flushbuffered(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  debug(3, "Connection %d: FLUSHBUFFERED %s : Content-Length %d", conn->connection_number,
        req->path, req->contentlength);
  debug_log_rtsp_message(2, "FLUSHBUFFERED request", req);

  uint64_t flushFromSeq = 0;
  uint64_t flushFromTS = 0;
  uint64_t flushUntilSeq = 0;
  uint64_t flushUntilTS = 0;
  int flushFromValid = 0;
  plist_t messagePlist = plist_from_rtsp_content(req);
  plist_t item = plist_dict_get_item(messagePlist, "flushFromSeq");
  if (item == NULL) {
    debug(2, "Can't find a flushFromSeq");
  } else {
    flushFromValid = 1;
    plist_get_uint_val(item, &flushFromSeq);
    debug(2, "flushFromSeq is %" PRId64 ".", flushFromSeq);
  }

  item = plist_dict_get_item(messagePlist, "flushFromTS");
  if (item == NULL) {
    if (flushFromValid != 0)
      debug(1, "flushFromSeq without flushFromTS!");
    else
      debug(2, "Can't find a flushFromTS");
  } else {
    plist_get_uint_val(item, &flushFromTS);
    if (flushFromValid == 0)
      debug(1, "flushFromTS without flushFromSeq!");
    debug(2, "flushFromTS is %" PRId64 ".", flushFromTS);
  }

  item = plist_dict_get_item(messagePlist, "flushUntilSeq");
  if (item == NULL) {
    debug(1, "Can't find the flushUntilSeq");
  } else {
    plist_get_uint_val(item, &flushUntilSeq);
    debug(2, "flushUntilSeq is %" PRId64 ".", flushUntilSeq);
  }

  item = plist_dict_get_item(messagePlist, "flushUntilTS");
  if (item == NULL) {
    debug(1, "Can't find the flushUntilTS");
  } else {
    plist_get_uint_val(item, &flushUntilTS);
    debug(2, "flushUntilTS is %" PRId64 ".", flushUntilTS);
  }

  debug_mutex_lock(&conn->flush_mutex, 1000, 1);
  // a flush with from... components will not be followed by a setanchor (i.e. a
  // play) if it's a flush that will be followed by a setanchor (i.e. a play)
  // then stop play now.
  if (flushFromValid == 0)
    conn->ap2_play_enabled = 0;

  // add the exact request as made to the linked list (not used for anything but
  // diagnostics now) int flushNow = 0; if (flushFromValid == 0)
  //  flushNow = 1;
  // add_flush_request(flushNow, flushFromSeq, flushFromTS, flushUntilSeq,
  // flushUntilTS, conn);

  // now, if it's an immediate flush, replace the existing request, if any
  // but it if's a deferred flush and there is an existing deferred request,
  // only update the flushUntil stuff -- that seems to preserve
  // the intended semantics

  // so, always replace these
  conn->ap2_flush_until_sequence_number = flushUntilSeq;
  conn->ap2_flush_until_rtp_timestamp = flushUntilTS;

  if ((conn->ap2_flush_requested != 0) && (conn->ap2_flush_from_valid != 0) &&
      (flushFromValid != 0)) {
    // if there is a request already, and it's a deferred request, and the
    // current request is also deferred... do nothing! -- leave the starting
    // point in place. Yeah, yeah, we know de Morgan's Law, but this seems
    // clearer
  } else {
    conn->ap2_flush_from_sequence_number = flushFromSeq;
    conn->ap2_flush_from_rtp_timestamp = flushFromTS;
  }

  conn->ap2_flush_from_valid = flushFromValid;
  conn->ap2_flush_requested = 1;

  // reflect the possibly updated flush request
  // add_flush_request(flushNow, conn->ap2_flush_from_sequence_number,
  // conn->ap2_flush_from_rtp_timestamp, conn->ap2_flush_until_sequence_number,
  // conn->ap2_flush_until_rtp_timestamp, conn);

  debug_mutex_unlock(&conn->flush_mutex, 3);

  if (flushFromValid)
    debug(2, "Deferred Flush Requested");
  else
    debug(2, "Immediate Flush Requested");

  // display_all_flush_requests(conn);

  resp->respcode = 200;
}

void handle_setrateanchori(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
  debug(3, "Connection %d: SETRATEANCHORI %s :: Content-Length %d", conn->connection_number,
        req->path, req->contentlength);

  plist_t messagePlist = plist_from_rtsp_content(req);

  if (messagePlist != NULL) {
    pthread_cleanup_push(plist_cleanup, (void *)messagePlist);
    plist_t item = plist_dict_get_item(messagePlist, "networkTimeSecs");
    if (item != NULL) {
      plist_t item_2 = plist_dict_get_item(messagePlist, "networkTimeTimelineID");
      if (item_2 == NULL) {
        debug(1, "Can't identify the Clock ID of the player.");
      } else {
        uint64_t nid;
        plist_get_uint_val(item_2, &nid);
        debug(2, "networkTimeTimelineID \"%" PRIx64 "\".", nid);
        conn->networkTimeTimelineID = nid;
      }
      uint64_t networkTimeSecs;
      plist_get_uint_val(item, &networkTimeSecs);
      debug(2, "anchor networkTimeSecs is %" PRIu64 ".", networkTimeSecs);

      item = plist_dict_get_item(messagePlist, "networkTimeFrac");
      uint64_t networkTimeFrac;
      plist_get_uint_val(item, &networkTimeFrac);
      debug(2, "anchor networkTimeFrac is 0%" PRIu64 ".", networkTimeFrac);
      // it looks like the networkTimeFrac is a fraction where the msb is work
      // 1/2, the next 1/4 and so on now, convert the network time and fraction
      // into nanoseconds
      networkTimeFrac = networkTimeFrac >> 32;
      networkTimeFrac = networkTimeFrac * 1000000000;
      networkTimeFrac = networkTimeFrac >> 32; // we should now be left with the ns

      networkTimeSecs = networkTimeSecs * 1000000000; // turn the whole seconds into ns
      uint64_t anchorTimeNanoseconds = networkTimeSecs + networkTimeFrac;

      debug(2, "anchorTimeNanoseconds looks like %" PRIu64 ".", anchorTimeNanoseconds);

      item = plist_dict_get_item(messagePlist, "rtpTime");
      uint64_t rtpTime;

      plist_get_uint_val(item, &rtpTime);
      // debug(1, "anchor rtpTime is %" PRId64 ".", rtpTime);
      uint32_t anchorRTPTime = rtpTime;

      int32_t added_latency = (int32_t)(config.audio_backend_latency_offset * conn->input_rate);
      // debug(1,"anchorRTPTime: %" PRIu32 ", added latency: %" PRId32 ".",
      // anchorRTPTime, added_latency);
      set_ptp_anchor_info(conn, conn->networkTimeTimelineID, anchorRTPTime - added_latency,
                          anchorTimeNanoseconds);
    }

    item = plist_dict_get_item(messagePlist, "rate");
    if (item != NULL) {
      uint64_t rate;
      plist_get_uint_val(item, &rate);
      debug(3, "anchor rate 0x%016" PRIx64 ".", rate);
      debug_mutex_lock(&conn->flush_mutex, 1000, 1);
      pthread_cleanup_push(mutex_unlock, &conn->flush_mutex);
      conn->ap2_rate = rate;
      if ((rate & 1) != 0) {
        debug(2, "Connection %d: Start playing.", conn->connection_number);
        activity_monitor_signify_activity(1);
        conn->ap2_play_enabled = 1;
      } else {
        debug(2, "Connection %d: Stop playing.", conn->connection_number);
        activity_monitor_signify_activity(0);
        conn->ap2_play_enabled = 0;
      }
      pthread_cleanup_pop(1); // unlock the conn->flush_mutex
    }
    pthread_cleanup_pop(1); // plist_free the messagePlist;
  } else {
    debug(1, "missing plist!");
  }
  resp->respcode = 200;
}

void *buffered_tcp_reader(void *arg) {
  pthread_cleanup_push(buffered_tcp_reader_cleanup_handler, NULL);
  buffered_tcp_desc *descriptor = (buffered_tcp_desc *)arg;

  listen(descriptor->sock_fd, 5);
  ssize_t nread;
  SOCKADDR remote_addr;
  memset(&remote_addr, 0, sizeof(remote_addr));
  socklen_t addr_size = sizeof(remote_addr);
  int finished = 0;
  int fd = accept(descriptor->sock_fd, (struct sockaddr *)&remote_addr, &addr_size);
  intptr_t pfd = fd;
  pthread_cleanup_push(socket_cleanup, (void *)pfd);

  do {
    if (pthread_mutex_lock(&descriptor->mutex) != 0)
      debug(1, "problem with mutex");
    pthread_cleanup_push(mutex_unlock, (void *)&descriptor->mutex);
    while ((descriptor->buffer_occupancy == descriptor->buffer_max_size) ||
           (descriptor->error_code != 0) || (descriptor->closed != 0)) {
      if (pthread_cond_wait(&descriptor->not_full_cv, &descriptor->mutex))
        debug(1, "Error waiting for buffered read");
    }
    pthread_cleanup_pop(1); // release the mutex

    // now we know it is not full, so go ahead and try to read some more into it

    // wrap
    if ((size_t)(descriptor->eoq - descriptor->buffer) == descriptor->buffer_max_size)
      descriptor->eoq = descriptor->buffer;

    // figure out how much to ask for
    size_t bytes_to_request = STANDARD_PACKET_SIZE;
    size_t free_space = descriptor->buffer_max_size - descriptor->buffer_occupancy;
    if (bytes_to_request > free_space)
      bytes_to_request = free_space; // don't ask for more than will fit

    size_t gap_to_end_of_buffer =
        descriptor->buffer + descriptor->buffer_max_size - descriptor->eoq;
    if (gap_to_end_of_buffer < bytes_to_request)
      bytes_to_request = gap_to_end_of_buffer; // only ask for what will fill to
                                               // the top of the buffer

    // do the read
    // debug(1, "Request buffered read  of up to %d bytes.", bytes_to_request);
    nread = recv(fd, descriptor->eoq, bytes_to_request, 0);
    // debug(1, "Received %d bytes for a buffer size of %d bytes.",nread,
    // descriptor->buffer_occupancy + nread);
    if (pthread_mutex_lock(&descriptor->mutex) != 0)
      debug(1, "problem with not empty mutex");
    pthread_cleanup_push(mutex_unlock, (void *)&descriptor->mutex);
    if (nread < 0) {
      char errorstring[1024];
      strerror_r(errno, (char *)errorstring, sizeof(errorstring));
      debug(1, "error in buffered_tcp_reader %d: \"%s\". Could not recv a packet.", errno,
            errorstring);
      descriptor->error_code = errno;
    } else if (nread == 0) {
      descriptor->closed = 1;
    } else if (nread > 0) {
      descriptor->eoq += nread;
      descriptor->buffer_occupancy += nread;
    } else {
      debug(1, "buffered audio port closed!");
    }
    // signal if we got data or an error or the file closed
    if (pthread_cond_signal(&descriptor->not_empty_cv))
      debug(1, "Error signalling");
    pthread_cleanup_pop(1); // release the mutex
  } while (finished == 0);

  debug(1, "Buffered TCP Reader Thread Exit \"Normal\" Exit Begin.");
  pthread_cleanup_pop(1); // close the socket
  pthread_cleanup_pop(1); // cleanup
  debug(1, "Buffered TCP Reader Thread Exit \"Normal\" Exit -- Shouldn't happen!.");
  pthread_exit(NULL);
}

void *rtp_ap2_control_receiver(void *arg) {
  pthread_cleanup_push(rtp_ap2_control_handler_cleanup_handler, arg);
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;
  uint8_t packet[4096];
  ssize_t nread;
  int keep_going = 1;
  uint64_t start_time = get_absolute_time_in_ns();
  uint64_t packet_number = 0;
  while (keep_going) {
    SOCKADDR from_sock_addr;
    socklen_t from_sock_addr_length = sizeof(SOCKADDR);
    memset(&from_sock_addr, 0, sizeof(SOCKADDR));

    nread = recvfrom(conn->ap2_control_socket, packet, sizeof(packet), 0,
                     (struct sockaddr *)&from_sock_addr, &from_sock_addr_length);
    uint64_t time_now = get_absolute_time_in_ns();

    int64_t time_since_start = time_now - start_time;

    if (nread > 0) {
      if ((time_since_start < 2000000) && ((packet[0] & 0x10) == 0)) {
        debug(1,
              "Dropping what looks like a (non-sentinel) packet left over from "
              "a previous session "
              "at %f ms.",
              0.000001 * time_since_start);
      } else {
        packet_number++;

        if (packet_number == 1) {
          if ((packet[0] & 0x10) != 0) {
            debug(2, "First packet is a sentinel packet.");
          } else {
            debug(1, "First packet is a not a sentinel packet!");
          }
        }
        // debug(1,"rtp_ap2_control_receiver coded: %u, %u", packet[0],
        // packet[1]);

        // store the from_sock_addr if we haven't already done so
        // v remember to zero this when you're finished!
        if (conn->ap2_remote_control_socket_addr_length == 0) {
          memcpy(&conn->ap2_remote_control_socket_addr, &from_sock_addr, from_sock_addr_length);
          conn->ap2_remote_control_socket_addr_length = from_sock_addr_length;
        }
        switch (packet[1]) {
          case 215: // code 215, effectively an anchoring announcement
          {
            // struct timespec tnr;
            // clock_gettime(CLOCK_REALTIME, &tnr);
            // uint64_t local_realtime_now = timespec_to_ns(&tnr);

            /*
                      char obf[4096];
                      char *obfp = obf;
                      int obfc;
                      for (obfc=0;obfc<nread;obfc++) {
                        snprintf(obfp, 3, "%02X", packet[obfc]);
                        obfp+=2;
                      };
                      *obfp=0;
                      debug(1,"AP2 Timing Control Received: \"%s\"",obf);
            */

            uint64_t remote_packet_time_ns = nctoh64(packet + 8);
            check64conversion("remote_packet_time_ns", packet + 8, remote_packet_time_ns);
            uint64_t clock_id = nctoh64(packet + 20);
            check64conversion("clock_id", packet + 20, clock_id);

            // debug(1, "we have clock_id: %" PRIx64 ".", clock_id);
            // debug(1,"remote_packet_time_ns: %" PRIx64 ",
            // local_realtime_now_ns: %" PRIx64 ".", remote_packet_time_ns,
            // local_realtime_now);
            uint32_t frame_1 =
                nctohl(packet + 4); // this seems to be the frame with latency of 77165 included
            check32conversion("frame_1", packet + 4, frame_1);
            uint32_t frame_2 =
                nctohl(packet + 16); // this seems to be the frame the time refers to
            check32conversion("frame_2", packet + 16, frame_2);
            // this just updates the anchor information contained in the packet
            // the frame and its remote time
            // add in the audio_backend_latency_offset;
            int32_t notified_latency = frame_2 - frame_1;
            if (notified_latency != 77175)
              debug(1, "Notified latency is %d frames.", notified_latency);
            int32_t added_latency =
                (int32_t)(config.audio_backend_latency_offset * conn->input_rate);
            // the actual latency is the notified latency plus the fixed latency
            // + the added latency

            int32_t net_latency =
                notified_latency + 11035 + added_latency; // this is the latency between
                                                          // incoming frames and the DAC
            net_latency = net_latency -
                          (int32_t)(config.audio_backend_buffer_desired_length * conn->input_rate);
            // debug(1, "Net latency is %d frames.", net_latency);

            if (net_latency <= 0) {
              if (conn->latency_warning_issued == 0) {
                warn("The stream latency (%f seconds) it too short to "
                     "accommodate an offset of %f "
                     "seconds and a backend buffer of %f seconds.",
                     ((notified_latency + 11035) * 1.0) / conn->input_rate,
                     config.audio_backend_latency_offset,
                     config.audio_backend_buffer_desired_length);
                warn("(FYI the stream latency needed would be %f seconds.)",
                     config.audio_backend_buffer_desired_length -
                         config.audio_backend_latency_offset);
                conn->latency_warning_issued = 1;
              }
              conn->latency = notified_latency + 11035;
            } else {
              conn->latency = notified_latency + 11035 + added_latency;
            }

            /*
            debug_mutex_lock(&conn->reference_time_mutex, 1000, 0);
            conn->remote_reference_timestamp_time = remote_packet_time_ns;
            conn->reference_timestamp =
                frame_1 - 11035 - added_latency; // add the latency in to the
            anchortime debug_mutex_unlock(&conn->reference_time_mutex, 0);
            */
            // this is now only used for calculating when to ask for resends
            // debug(1, "conn->latency is %d.", conn->latency);
            // debug(1,"frame_1: %" PRIu32 ", added latency: %" PRId32 ".",
            // frame_1, added_latency);
            set_ptp_anchor_info(conn, clock_id, frame_1 - 11035 - added_latency,
                                remote_packet_time_ns);
          } break;
          case 0xd6:
            // six bytes in is the sequence number at the start of the encrypted
            // audio packet returns the sequence number but we're not really
            // interested
            decipher_player_put_packet(packet + 6, nread - 6, conn);
            break;
          default: {
            char *packet_in_hex_cstring =
                debug_malloc_hex_cstring(packet, nread); // remember to free this afterwards
            debug(1,
                  "AP2 Control Receiver Packet of first byte 0x%02X, type "
                  "0x%02X length %d received: "
                  "\"%s\".",
                  packet[0], packet[1], nread, packet_in_hex_cstring);
            free(packet_in_hex_cstring);
          } break;
        }
      }

    } else if (nread == 0) {
      debug(1, "AP2 Control Receiver -- connection closed.");
      keep_going = 0;
    } else {
      debug(1, "AP2 Control Receiver -- error %d receiving a packet.", errno);
    }
  }
  debug(1, "AP2 Control RTP thread \"normal\" exit -- this can't happen. Hah!");
  pthread_cleanup_pop(1);
  debug(1, "AP2 Control RTP thread exit.");
  pthread_exit(NULL);
}

void check64conversion(const char *prompt, const uint8_t *source, uint64_t value) {
  char converted_value[128];
  sprintf(converted_value, "%" PRIx64 "", value);

  char obf[32];
  char *obfp = obf;
  int obfc;
  int suppress_zeroes = 1;
  for (obfc = 0; obfc < 8; obfc++) {
    if ((suppress_zeroes == 0) || (source[obfc] != 0)) {
      if (suppress_zeroes != 0) {
        if (source[obfc] < 0x10) {
          snprintf(obfp, 3, "%1x", source[obfc]);
          obfp += 1;
        } else {
          snprintf(obfp, 3, "%02x", source[obfc]);
          obfp += 2;
        }
      } else {
        snprintf(obfp, 3, "%02x", source[obfc]);
        obfp += 2;
      }
      suppress_zeroes = 0;
    }
  };
  *obfp = 0;
  if (strcmp(converted_value, obf) != 0) {
    debug(1, "%s check64conversion error converting \"%s\" to %" PRIx64 ".", prompt, obf, value);
  }
}

void check32conversion(const char *prompt, const uint8_t *source, uint32_t value) {
  char converted_value[128];
  sprintf(converted_value, "%" PRIx32 "", value);

  char obf[32];
  char *obfp = obf;
  int obfc;
  int suppress_zeroes = 1;
  for (obfc = 0; obfc < 4; obfc++) {
    if ((suppress_zeroes == 0) || (source[obfc] != 0)) {
      if (suppress_zeroes != 0) {
        if (source[obfc] < 0x10) {
          snprintf(obfp, 3, "%1x", source[obfc]);
          obfp += 1;
        } else {
          snprintf(obfp, 3, "%02x", source[obfc]);
          obfp += 2;
        }
      } else {
        snprintf(obfp, 3, "%02x", source[obfc]);
        obfp += 2;
      }
      suppress_zeroes = 0;
    }
  };
  *obfp = 0;
  if (strcmp(converted_value, obf) != 0) {
    debug(1, "%s check32conversion error converting \"%s\" to %" PRIx32 ".", prompt, obf, value);
  }
}

void *rtp_event_receiver(void *arg) {
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;
  debug(2, "Connection %d: AP2 Event Receiver started", conn->connection_number);

  listen(conn->event_socket, 5);

  uint8_t packet[4096];
  ssize_t nread;
  SOCKADDR remote_addr;
  memset(&remote_addr, 0, sizeof(remote_addr));
  socklen_t addr_size = sizeof(remote_addr);

  int fd = accept(conn->event_socket, (struct sockaddr *)&remote_addr, &addr_size);
  intptr_t pfd = fd;
  pthread_cleanup_push(socket_cleanup, (void *)pfd);
  int finished = 0;
  do {
    nread = recv(fd, packet, sizeof(packet), 0);

    if (nread < 0) {
      char errorstring[1024];
      strerror_r(errno, (char *)errorstring, sizeof(errorstring));
      debug(1,
            "Connection %d: error in ap2 rtp_event_receiver %d: \"%s\". Could "
            "not recv a packet.",
            conn->connection_number, errno, errorstring);

    } else if (nread > 0) {
      // ssize_t plen = nread;
      debug(1, "Packet Received on Event Port.");
      if (packet[1] == 0xD7) {
        debug(1,
              "Connection %d: AP2 Event Receiver -- Time Announce RTP packet "
              "of type 0x%02X length "
              "%d received.",
              conn->connection_number, packet[1], nread);
      } else {
        debug(1,
              "Connection %d: AP2 Event Receiver -- Unknown RTP packet of type "
              "0x%02X length %d "
              "received.",
              conn->connection_number, packet[1], nread);
      }

    } else {
      finished = 1;
    }
  } while (finished == 0);
  pthread_cleanup_pop(1); // close the socket
  pthread_cleanup_pop(1); // do the cleanup
  debug(2, "Connection %d: AP2 Event Receiver RTP thread \"normal\" exit.",
        conn->connection_number);
  pthread_exit(NULL);
}

void *player_watchdog_thread_code(void *arg) {
  pthread_cleanup_push(player_watchdog_thread_cleanup_handler, arg);
  rtsp_conn_info *conn = (rtsp_conn_info *)arg;
  do {
    usleep(2000000); // check every two seconds
                     // debug(3, "Connection %d: Check the thread is doing
                     // something...", conn->connection_number);

  } while (1);
  pthread_cleanup_pop(0); // should never happen
  pthread_exit(NULL);
}

void *activity_monitor_thread_code(void *arg) {
  int rc = pthread_mutex_init(&activity_monitor_mutex, NULL);
  if (rc)
    die("activity_monitor: error %d initialising activity_monitor_mutex.", rc);

  rc = pthread_cond_init(&activity_monitor_cv, NULL);
  if (rc)
    die("activity_monitor: error %d initialising activity_monitor_cv.");
  pthread_cleanup_push(activity_thread_cleanup_handler, arg);

  uint64_t sec;
  uint64_t nsec;
  struct timespec time_for_wait;

  state = am_inactive;
  player_state = ps_inactive;

  pthread_mutex_lock(&activity_monitor_mutex);
  do {
    switch (state) {
      case am_inactive:
        debug(2, "am_state: am_inactive");
        while (player_state != ps_active)
          pthread_cond_wait(&activity_monitor_cv, &activity_monitor_mutex);
        state = am_active;
        debug(2, "am_state: going active");
        break;
      case am_active:
        // debug(1,"am_state: am_active");
        while (player_state != ps_inactive)
          pthread_cond_wait(&activity_monitor_cv, &activity_monitor_mutex);
        if (config.active_state_timeout == 0.0) {
          state = am_inactive;
        } else {
          state = am_timing_out;

          uint64_t time_to_wait_for_wakeup_ns =
              (uint64_t)(config.active_state_timeout * 1000000000);

          uint64_t time_of_wakeup_ns = get_realtime_in_ns() + time_to_wait_for_wakeup_ns;
          sec = time_of_wakeup_ns / 1000000000;
          nsec = time_of_wakeup_ns % 1000000000;
          time_for_wait.tv_sec = sec;
          time_for_wait.tv_nsec = nsec;
        }
        break;
      case am_timing_out:
        rc = 0;
        while ((player_state != ps_active) && (rc != ETIMEDOUT)) {
          rc = pthread_cond_timedwait(&activity_monitor_cv, &activity_monitor_mutex,
                                      &time_for_wait); // this is a pthread cancellation point
        }
        if (player_state == ps_active)
          state = am_active; // player has gone active -- do nothing, because it's
                             // still active
        else if (rc == ETIMEDOUT) {
          state = am_inactive;
          pthread_mutex_unlock(&activity_monitor_mutex);
          going_inactive(0); // don't wait for completion -- it makes no sense
          pthread_mutex_lock(&activity_monitor_mutex);
        } else {
          // activity monitor was woken up in the state am_timing_out, but not by
          // a timeout and player is not in ps_active state
          debug(1, "activity monitor was woken up in the state am_timing_out, "
                   "but didn't change state");
        }
        break;
      default:
        debug(1, "activity monitor in an illegal state!");
        state = am_inactive;
        break;
    }
  } while (1);
  pthread_mutex_unlock(&activity_monitor_mutex);
  pthread_cleanup_pop(0); // should never happen
  pthread_exit(NULL);
}

int player_prepare_to_play(rtsp_conn_info *conn) {
  // need to use conn in place of stream below. Need to put the stream as a
  // parameter to he
  if (conn->player_thread != NULL)
    die("Trying to create a second player thread for this RTSP session");
  if (config.buffer_start_fill > BUFFER_FRAMES)
    die("specified buffer starting fill %d > buffer size %d", config.buffer_start_fill,
        BUFFER_FRAMES);
  // active, and should be before play's command hook, command_start()

  conn->input_bytes_per_frame = 4; // default -- may be changed later
  // call on the output device to prepare itself
  if ((config.output) && (config.output->prepare))
    config.output->prepare();
  return 0;
}

int player_play(rtsp_conn_info *conn) {
  pthread_t *pt = malloc(sizeof(pthread_t));
  if (pt == NULL)
    die("Couldn't allocate space for pthread_t");
  conn->player_thread = pt;
  int rc = pthread_create(pt, NULL, player_thread_func, (void *)conn);
  if (rc)
    debug(1, "Error creating player_thread: %s", strerror(errno));

  return 0;
}

int player_stop(rtsp_conn_info *conn) {
  // note -- this may be called from another connection thread.
  // int dl = debuglev;
  // debuglev = 3;
  debug(3, "player_stop");
  if (conn->player_thread) {
    debug(3, "player_thread cancel...");
    pthread_cancel(*conn->player_thread);
    debug(3, "player_thread join...");
    if (pthread_join(*conn->player_thread, NULL) == -1) {
      char errorstring[1024];
      strerror_r(errno, (char *)errorstring, sizeof(errorstring));
      debug(1, "Connection %d: error %d joining player thread: \"%s\".", conn->connection_number,
            errno, (char *)errorstring);
    } else {
      debug(3, "player_thread joined.");
    }
    free(conn->player_thread);
    conn->player_thread = NULL;

    // debuglev = dl;

    return 0;
  } else {
    debug(3, "Connection %d: player thread already deleted.", conn->connection_number);
    // debuglev = dl;
    return -1;
  }
}