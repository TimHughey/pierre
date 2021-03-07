/*
Pierre - Custom audio processing for light shows at Wiss Landing
Copyright (C) 2021  Tim Hughey

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include <iostream>

#include <sys/resource.h>
#include <time.h>

#include "audio/audiotx.hpp"

using namespace std;
using namespace pierre;

int main(int argc, char *argv[]) {
  string_t dest_host;

  cerr << "\nHello, this is Pierre.\n\n";

  if (argc > 1) {
    dest_host = argv[1];
  } else {
    dest_host = "test-with-devs.ruth.wisslanding.com";
  }

  auto pid = getpid();
  setpriority(PRIO_PROCESS, pid, -10);

  AudioTx_t audio_tx(dest_host);

  if (audio_tx.init()) {
    audio_tx.run();
  }

  return 0;
}
