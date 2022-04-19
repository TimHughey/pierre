
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

#include "rtsp/reply/command.hpp"
#include "rtsp/reply.hpp"

namespace pierre {
namespace rtsp {

Command::Command(const Reply::Opts &opts) : Reply(opts), Aplist(requestContent()) {
  // maybe more
}

bool Command::populate() {
  // NOTE
  //
  // /command msgs come in two flavors:
  //  1. plist with specific keys

  responseCode(OK); // default

  if (checkUpdateSupportedCommands() == false) {
    responseCode(BadRequest);
    dictDump();
    return true;
  }

  return true;
}

bool Command::checkUpdateSupportedCommands() {
  if (dictCompareString("type", "updateMRSupportedCommands")) {
    ArrayStrings array;

    if (dictGetStringArray("params", "mrSupportedCommandsFromSender", array) == false) {
      return false;
    }
  }

  return true;
}

// we have a plist -- try to get the dict item keyed to
// "updateMRSupportedCommands"
// plist_t item = plist_dict_get_item(command_dict, "type");
// if (item != NULL) {
//   char *typeValue = NULL;
//   plist_get_string_val(item, &typeValue);
//   if (typeValue && (strcmp(typeValue, "updateMRSupportedCommands") == 0)) {
//     item = plist_dict_get_item(command_dict, "params");
//     if (item != NULL) {
//       // the item should be a dict
//       plist_t item_array = plist_dict_get_item(item, "mrSupportedCommandsFromSender");
//       if (item_array != NULL) {
//         // here we have an array of data items
//         uint32_t items = plist_array_get_size(item_array);
//         if (items) {
//           uint32_t item_number;
//           for (item_number = 0; item_number < items; item_number++) {
//             plist_t the_item = plist_array_get_item(item_array, item_number);
//             char *buff = NULL;
//             uint64_t length = 0;
//             plist_get_data_val(the_item, &buff, &length);
//             // debug(1,"Item %d, length: %" PRId64 " bytes", item_number,
//             // length);
//             if (buff && (length >= strlen("bplist00")) && (strstr(buff, "bplist00") == buff)) {
//               // debug(1,"Contains a plist.");
//               plist_t subsidiary_plist = NULL;
//               plist_from_memory(buff, length, &subsidiary_plist);
//               if (subsidiary_plist) {
//                 char *printable_plist = plist_content(subsidiary_plist);
//                 if (printable_plist) {
//                   debug(3, "\n%s", printable_plist);
//                   free(printable_plist);
//                 } else {
//                   debug(1, "Can't print the plist!");
//                 }
//                 // plist_free(subsidiary_plist);
//               } else {
//                 debug(1, "Can't access the plist!");
//               }
//             }
//           }
//         }
//       }
//     }
//     resp->respcode = 400; // say it's a bad request
//   }
// }

} // namespace rtsp
} // namespace pierre