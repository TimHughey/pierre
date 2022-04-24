
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
#include <iterator>
#include <netinet/in.h>
#include <string>
#include <sys/mman.h>
#include <sys/socket.h>
#include <thread>
#include <vector>

#include "core/service.hpp"
#include "nptp/nptp.hpp"

namespace pierre {
namespace rtsp {

using std::runtime_error;

// using namespace std;

Nptp::Nptp(sService service) {
  const auto &name = service->name();
  const auto &device_id = service->deviceID();

  _shm_name = fmt::format("/{}-{}", name, device_id);

  resetPeerList(); // registers _shm_name with nqptp
  openAndMap();    // gains access to shared memory segment
}

bool Nptp::isMapped() const { return ((_mapped != nullptr) && (_mapped != MAP_FAILED)); }

void Nptp::openAndMap() {
  if (isMapped() == false) {
    // fmt::print("opening {}\n", _shm_name, .c_str());
    _shm_fd = shm_open(_shm_name.c_str(), O_RDWR, 0);

    if (_shm_fd >= 0) {
      // flags must be PROT_READ | PROT_WRITE to allow writes the mapped memory
      //  for the mutex
      constexpr auto flags = PROT_READ | PROT_WRITE;
      constexpr auto bytes = sizeof(shm_structure);

      _mapped = mmap(NULL, bytes, flags, MAP_SHARED, _shm_fd, 0);

      _ok = ((_mapped == MAP_FAILED) ? false : true);

      // close the shared memory fd regardless of mmap result
      _ok = (close(_shm_fd) >= 0) ? true : false;
      _shm_fd = -1;
    }

    // fmt::print("_shm_fd={} _mapped={} _ok={}\n", _shm_fd, _mapped, _ok);
  }
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

void Nptp::sendTimingPeers(const std::vector<std::string> &peers) {
  const string peer_list = fmt::format("T {}", fmt::join(peers, " "));

  // fmt::print("peer list={}\n", peer_list);

  sendCtrlMsg(peer_list);
}

void Nptp::sendCtrlMsg(const string msg) {
  // std::vector<char> full_msg;
  // auto where = std::back_inserter(full_msg);

  const string full_msg = fmt::format("{} {}", _shm_name, msg);

  // NOTE: not worth pulling in boost::asio for this quick datagram
  // create a datagram socket in the internet domain and use the
  // default protocol (UDP).
  int s;

  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    constexpr auto msg = "unable to open a socket to NQPTP";
    fmt::print(msg);
    throw(runtime_error(msg));
  }

  // set up the packet destination info
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(_ctrl_port); // server port
  server.sin_addr.s_addr = 0;          // server address (unspecified)

  const auto buf = full_msg.data();
  const auto bytes = full_msg.size();

  // sendto requires a sockaddr struct
  auto addr = reinterpret_cast<struct sockaddr *>(&server);
  constexpr auto addr_len = sizeof(server);

  if (sendto(s, buf, bytes, 0, addr, addr_len) < 0) {
    fmt::print("error sending peer list");
    throw(runtime_error("error sending peer list"));
  }
  /* Deallocate the socket */
  close(s);
}

void Nptp::start() {
  _thread = std::thread([this]() { runLoop(); });
  _handle = _thread.native_handle();

  pthread_setname_np(_handle, "NPTP");
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &_prev_state);
}

void Nptp::unMap() {
  if (_mapped && (_mapped != MAP_FAILED)) {
    constexpr auto bytes = sizeof(shm_structure);

    munmap(_mapped, bytes);
    _shm_fd = -1;
    _mapped = nullptr;
  }

  if (_shm_fd >= 0) {
    close(_shm_fd);
    _shm_fd = -1;
  }
}

} // namespace rtsp
} // namespace pierre