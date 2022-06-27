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

#include "reply/set_anchor.hpp"
#include "player/player.hpp"
#include "reply/dict_keys.hpp"
#include "rtp_time/anchor/data.hpp"

namespace pierre {
namespace airplay {
namespace reply {

using namespace packet;

bool SetAnchor::populate() {
  rdict = plist();

  __LOGX("{:<18} DICT DUMP {}\n", moduleId, rdict.inspect());

  saveAnchorInfo();

  responseCode(RespCode::OK);

  return true;
}

void SetAnchor::saveAnchorInfo() {
  // a complete anchor message contains these keys
  const Aplist::KeyList keys{dk::RATE,          dk::NET_TIMELINE_ID, dk::NET_TIME_SECS,
                             dk::NET_TIME_FRAC, dk::NET_TIME_FLAGS,  dk::RTP_TIME};

  if (rdict.existsAll(keys)) {
    // this is a complete anchor set
    auto anchor_data = anchor::Data{.rate = rdict.uint({dk::RATE}),
                                    .clockID = rdict.uint({dk::NET_TIMELINE_ID}),
                                    .secs = rdict.uint({dk::NET_TIME_SECS}),
                                    .frac = rdict.uint({dk::NET_TIME_FRAC}),
                                    .flags = rdict.uint({dk::NET_TIME_FLAGS}),
                                    .rtpTime = rdict.uint({dk::RTP_TIME})};

    Player::saveAnchor(anchor_data);
  } else {
    auto anchor_data = anchor::Data{.rate = rdict.uint({dk::RATE})};
    Player::saveAnchor(anchor_data);
  }
}

} // namespace reply
} // namespace airplay
} // namespace pierre

// void handle_setrateanchori(rtsp_conn_info *conn, rtsp_message *req, rtsp_message *resp) {
//   debug(3, "Connection %d: SETRATEANCHORI %s :: Content-Length %d", conn->connection_number,
//         req->path, req->contentlength);

//   plist_t messagePlist = plist_from_rtsp_content(req);

//   if (messagePlist != NULL) {
//     pthread_cleanup_push(plist_cleanup, (void *)messagePlist);
//     plist_t item = plist_dict_get_item(messagePlist, "networkTimeSecs");
//     if (item != NULL) {
//       plist_t item_2 = plist_dict_get_item(messagePlist, "networkTimeTimelineID");
//       if (item_2 == NULL) {
//         debug(1, "Can't identify the Clock ID of the player.");
//       } else {
//         uint64_t nid;
//         plist_get_uint_val(item_2, &nid);
//         debug(2, "networkTimeTimelineID \"%" PRIx64 "\".", nid);
//         conn->networkTimeTimelineID = nid;
//       }
//       uint64_t networkTimeSecs;
//       plist_get_uint_val(item, &networkTimeSecs);
//       debug(2, "anchor networkTimeSecs is %" PRIu64 ".", networkTimeSecs);

//       item = plist_dict_get_item(messagePlist, "networkTimeFrac");
//       uint64_t networkTimeFrac;
//       plist_get_uint_val(item, &networkTimeFrac);
//       debug(2, "anchor networkTimeFrac is 0%" PRIu64 ".", networkTimeFrac);
//       // it looks like the networkTimeFrac is a fraction where the msb is work
//       // 1/2, the next 1/4 and so on now, convert the network time and fraction
//       // into nanoseconds
//       networkTimeFrac = networkTimeFrac >> 32; // reduce precision to about 1/4 nanosecond
//       networkTimeFrac = networkTimeFrac * 1000000000;
//       networkTimeFrac = networkTimeFrac >> 32; // we should now be left with the ns

//       networkTimeSecs = networkTimeSecs * 1000000000; // turn the whole seconds into ns
//       uint64_t anchorTimeNanoseconds = networkTimeSecs + networkTimeFrac;

//       debug(2, "anchorTimeNanoseconds looks like %" PRIu64 ".", anchorTimeNanoseconds);

//       item = plist_dict_get_item(messagePlist, "rtpTime");
//       uint64_t rtpTime;

//       plist_get_uint_val(item, &rtpTime);
//       // debug(1, "anchor rtpTime is %" PRId64 ".", rtpTime);
//       uint32_t anchorRTPTime = rtpTime;

//       int32_t added_latency = (int32_t)(config.audio_backend_latency_offset * conn->input_rate);
//       // debug(1,"anchorRTPTime: %" PRIu32 ", added latency: %" PRId32 ".",
//       // anchorRTPTime, added_latency);
//       set_ptp_anchor_info(conn, conn->networkTimeTimelineID, anchorRTPTime - added_latency,
//                           anchorTimeNanoseconds);
//     }

//     item = plist_dict_get_item(messagePlist, "rate");
//     if (item != NULL) {
//       uint64_t rate;
//       plist_get_uint_val(item, &rate);
//       debug(3, "anchor rate 0x%016" PRIx64 ".", rate);
//       debug_mutex_lock(&conn->flush_mutex, 1000, 1);
//       pthread_cleanup_push(mutex_unlock, &conn->flush_mutex);
//       conn->ap2_rate = rate;
//       if ((rate & 1) != 0) {
//         debug(2, "Connection %d: Start playing.", conn->connection_number);
//         activity_monitor_signify_activity(1);
//         conn->ap2_play_enabled = 1;
//       } else {
//         debug(2, "Connection %d: Stop playing.", conn->connection_number);
//         activity_monitor_signify_activity(0);
//         conn->ap2_play_enabled = 0;
//       }
//       pthread_cleanup_pop(1); // unlock the conn->flush_mutex
//     }
//     pthread_cleanup_pop(1); // plist_free the messagePlist;
//   } else {
//     debug(1, "missing plist!");
//   }
//   resp->respcode = 200;
// }