
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
//
//  This work based on and inspired by
//  https://github.com/mikebrady/nqptp Copyright (c) 2021--2022 Mike Brady.

#include <exception>
#include <fcntl.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <thread>

#include "rtsp/nptp.hpp"

namespace pierre {
namespace rtsp {

using std::runtime_error;

// using namespace std;

Nptp::Nptp(const string &app_name, sHost host)
    : _app_name(app_name), _thread{} {
  _interface_name = fmt::format("/{}-{}", app_name, host->deviceID());

  resetPeerList();
  openAndMap();

  // more later
}

Nptp::~Nptp() {
  if (_mapped && (_mapped != MAP_FAILED)) {
    constexpr auto bytes = sizeof(struct shm_structure);

    munmap(_mapped, bytes);
    _mapped = nullptr;
  }
}

void Nptp::openAndMap() {
  // fmt::print("opening {}\n", _interface_name.c_str());
  _shm_fd = shm_open(_interface_name.c_str(), O_RDWR, 0);

  if (_shm_fd >= 0) {
    // flags must be PROT_READ | PROT_WRITE to allow writes the mapped memory
    //  for the mutex
    constexpr auto flags = PROT_READ | PROT_WRITE;
    constexpr auto bytes = sizeof(struct shm_structure);

    _mapped = mmap(NULL, bytes, flags, MAP_SHARED, _shm_fd, 0);

    _ok = ((_mapped == MAP_FAILED) ? false : true);

    // close the shared memory fd regardless of mmap result
    _ok = (close(_shm_fd) >= 0) ? true : false;
  }

  // fmt::print("_shm_fd={} _mapped={} _ok={}\n", _shm_fd, _mapped, _ok);
}

void Nptp::runLoop() {
  do {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &_prev_state);

    std::unique_lock lk(_mutex);

    // fmt::print("Nptp::runLoop() cv={}\n", _ready);
    _condv.wait(lk, [this] { return _ready; });

    // fmt::print("Nptp::runLoop() cv={} \n", _ready);
  } while (true);
}

void Nptp::sendCtrlMsg(const char *msg) {
  auto full_msg = fmt::format("{} {}", _interface_name, msg);

  int s;
  unsigned short port = htons(_ctrl_port);
  struct sockaddr_in server;

  /* Create a datagram socket in the internet domain and use the
   * default protocol (UDP).
   */
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fmt::print("Can't open a socket to NQPTP");
    throw(runtime_error("could not open socket"));
  }

  /* Set up the server name */
  server.sin_family = AF_INET; /* Internet Domain    */
  server.sin_port = port;      /* Server Port        */
  server.sin_addr.s_addr = 0;  /* Server's Address   */

  /* Send the message in buf to the server */
  if (sendto(s, full_msg.c_str(), full_msg.size(), 0,
             (struct sockaddr *)&server, sizeof(server)) < 0) {
    fmt::print("error sending peer list");
    throw(runtime_error("error sending peer list"));
  }
  /* Deallocate the socket */
  close(s);
}

void Nptp::start() {
  _thread = std::thread([this]() { runLoop(); });
  _handle = _thread.native_handle();

  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_prev_state);
}

} // namespace rtsp
} // namespace pierre