// Pierre
// Copyright (C) 2022 Tim Hughey
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// https://www.wisslanding.com

#include "app.hpp"
#include "base/conf/cli_args.hpp"
#include "base/conf/fixed.hpp"
#include "base/crypto.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "base/stats.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/rtsp.hpp"

#include <boost/asio/signal_set.hpp>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <exception>
#include <filesystem>
#include <fmt/os.h>
#include <fmt/std.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>

static void daemonize(const std::string pid_file);
static bool pid_file_check(const std::string pid_file, bool remove_only = false);
static std::string pid_file_unlink(const std::string pid_file, pid_t pid = 0);

/// @brief primary entry point for application
/// @param argc number of cli args
/// @param argv actual cli args
/// @return exit code returned to starting process
int main(int argc, char *argv[]) {
  static constexpr const std::string_view mod_id{"main"};
  static constexpr const std::string_view fn_id{"main"};

  using namespace pierre;

  int rc{1}; // exit code, default to failed

  // initialize sodium and gcrypt
  crypto::init();

  // handle cli args, config parse
  conf::cli_args(argc, argv);

  // create the app object
  std::shared_ptr<App> app;

  if (conf::cli_args::nominal_start()) {
    // all is well, proceed with starting the app

    // become daemon (if requested)
    if (conf::fixed::daemon()) {
      daemonize(conf::fixed::pid_file());
    }

    app = App::create();

    app->main();
  } else {
    // the app isn't runnable for one of the following reasons:
    //  -cli help requested
    //  -cli args bad
    //  -configuration directory does not exist

    rc = 1;

    if (!conf::cli_args::error_msg().empty()) {
      std::cout << conf::cli_args::error_msg() << std::endl;
    }
  }

  // if app is created then we are assured command line args
  // are good, help was not requested and we should proceed to main()

  exit(rc);
}

void daemonize(const std::string pid_file) {
  using namespace pierre;

  pid_file_check(pid_file);

  const sigset_t block{SIGHUP};
  [[maybe_unused]] sigset_t prev;
  sigprocmask(SIG_BLOCK, &block, &prev);

  if (auto x = syscall(SYS_close_range, 3, ~0U, 0); x == -1) {
    perror("close_range failed");
    exit(EXIT_FAILURE);
  }

  auto child_pid = fork();

  // handle parent process or fork failure
  if (child_pid > 0) { // initial parent process, exit cleanly

    exit(EXIT_SUCCESS);
  } else if (child_pid < 0) { // fork failed
    perror("initial fork failed");
    exit(EXIT_FAILURE);
  }

  // this is child process 1
  setsid(); // create new session
  child_pid = fork();

  if (child_pid > 0) { // child process 1, exit cleanly
    exit(EXIT_SUCCESS);
  } else if (child_pid < 0) { // fork failed
    perror("second fork failed");
    exit(EXIT_FAILURE);
  }

  // actual daemon (child 2)
  stdin = std::freopen("/dev/null", "r", stdin);
  stdout = std::freopen("/dev/null", "a+", stdout);
  stderr = std::freopen("/dev/null", "a+", stderr);

  umask(0);

  using namespace std::filesystem;

  current_path(path("/"));

  const auto flags = fmt::file::WRONLY | fmt::file::CREATE | fmt::file::TRUNC;
  auto os = fmt::output_file(conf::fixed::pid_file().c_str(), flags);

  os.print("{}", getpid());
  os.close();
}

bool pid_file_check(const std::string pid_file, bool remove_only) {
  using namespace std::filesystem;
  using namespace pierre;

  bool rc{false};

  path file{pid_file};

  // pid file doesn't exist
  if (!exists(file)) return true;

  std::ifstream istrm(file);
  if (istrm.is_open()) {
    int64_t pid;

    istrm >> pid; // read the pid

    auto kill_rc = kill(pid, 0);

    if ((kill_rc == 0) && !conf::fixed::force_restart()) { // pid is alive and we can signal it
      std::cout << file << "contains live pid " << pid;
      std::cout << ", use --force-restart to restart" << std::endl;

    } else if (!remove_only && (kill_rc == 0)) {

      // attempt to kill the process for 3 seconds
      for (auto i = 0; (i < 3) && (kill_rc == 0); i++) {
        kill(pid, SIGINT);

        kill_rc = kill(pid, 0);
        if (kill_rc == 0) sleep(1);
      }

      // if the process appears to be dead then recurse to remove the pid file
      rc = (kill_rc != 0) && pid_file_check(pid_file, true);

    } else if ((errno == ESRCH) || remove_only) {
      const auto msg = pid_file_unlink(pid_file);
      rc = msg.empty();

      if (!rc) std::cout << msg << std::endl;

    } else {
      std::cout << file << "contains" << pid << ":" << std::strerror(errno) << std::endl;
    }
  }

  return rc;
}

std::string pid_file_unlink(const std::string pid_file, pid_t pid) {
  using namespace std::filesystem;

  std::string msg;
  int stored_pid{0};
  const path pid_path{pid_file};

  try {
    std::ifstream ifs(pid_path);
    ifs >> stored_pid;

    if ((pid == 0) || (stored_pid == pid)) {
      auto rc = remove(pid_path);
      msg = fmt::format("unlinked {} {}", pid_path, rc);
    } else if (stored_pid != pid) {
      msg = fmt::format("[WARN] stored pid({}) does not match requested pid({})", stored_pid, pid);
    }

  } catch (std::exception &e) {
    msg = fmt::format("[ERROR] unable to read stored pid: {}", e.what());
  }

  return msg;
}

////
//// App
////
namespace pierre {

App::App() noexcept : ss_shutdown(io_ctx, SIGINT) {}

void App::main() {
  static constexpr csv fn_id{"main"};

  auto main_pid = getpid();

  thread = std::jthread([main_pid, this, self = shared_from_this()](std::stop_token stoken) {
    Logger::create(io_ctx);

    INFO(module_id, "init", "sizeof={:>5}\n", sizeof(App));

    Stats::create(io_ctx);
    shared::mdns = std::make_unique<mDNS>();
    ss_shutdown.async_wait([self](const error_code &ec, int signal) {
      static constexpr csv fn_id{"shutdown"};
      if (ec) return;

      Logger::synchronous();
      INFO_AUTO("caught SIGINT({}) app_pid={}\n", signal, getpid());
      self->thread.request_stop();

      asio::post(self->io_ctx, [self]() { self->io_ctx.stop(); });
    });

    std::unique_ptr<Rtsp> rtsp;

    try {
      // create Rtsp under try/catch to handle port already in use
      rtsp = std::make_unique<Rtsp>();

    } catch (const std::exception &e) {
      Logger::synchronous();
      INFO_AUTO("run exception {}\n", e.what());
    }

    while (!stoken.stop_requested()) {
      self->io_ctx.run_for(1s);
    }

    self->io_ctx.stop();
    pid_file_unlink(conf::fixed::pid_file(), main_pid);

    INFO_AUTO("primary io_ctx has finished all work\n");

    rtsp.reset();

    shared::mdns.reset();

    Stats::shutdown();
    Logger::shutdown();
  });

  thread.join();
}

} // namespace pierre
