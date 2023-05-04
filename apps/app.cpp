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
#include "base/conf/watch.hpp"
#include "base/crypto.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "base/stats.hpp"
#include "base/types.hpp"
#include "desk/desk.hpp"
#include "frame/master_clock.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/rtsp.hpp"

#include <boost/asio/consign.hpp>
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
#include <optional>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

using namespace std::string_view_literals;

static void daemonize(const std::string pid_file);
static bool pid_file_check(const std::string pid_file, bool remove_only = false);
static std::string pid_file_unlink(std::filesystem::path pid_file, pid_t pid = 0);

/// @brief primary entry point for application
/// @param argc number of cli args
/// @param argv actual cli args
/// @return exit code returned to starting process
int main(int argc, char *argv[]) {
  constexpr auto mod_id{"main"sv};
  constexpr auto fn_id{"main"sv};

  using namespace pierre;

  int rc{1}; // exit code, default to failed

  // initialize sodium and gcrypt
  crypto::init();

  // handle cli args, config parse
  conf::cli_args(argc, argv);

  if (conf::cli_args::nominal_start()) {
    // all is well, proceed with app startup

    // become daemon (if requested)
    if (conf::fixed::daemon()) daemonize(conf::fixed::pid_file());

    // mDNS uses AvahiThreadedPoll (spawns a thread), therefore create it:
    // -AFTER daemonizing
    // -BEFORE App (to avoid needing io_context::notify_fork())
    shared::mdns = std::make_unique<mDNS>();

    // create the actual App
    pierre::App app;

    // run the app, runs when app is finished
    app.main();

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
      const auto msg = pid_file_unlink(file);
      rc = msg.empty();

      if (!rc) std::cout << msg << std::endl;

    } else {
      std::cout << file << "contains" << pid << ":" << std::strerror(errno) << std::endl;
    }
  }

  return rc;
}

std::string pid_file_unlink(std::filesystem::path pid_file, pid_t pid) {
  using namespace std::filesystem;

  std::string msg;
  int stored_pid{0};

  try {
    std::ifstream ifs(pid_file);
    ifs >> stored_pid;

    if ((pid == 0) || (stored_pid == pid)) {
      if (auto rc = remove(pid_file); !rc) {
        msg = fmt::format("[WARN] failed to remove {} contents={}", pid_file, stored_pid);
      }

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

using system_error = boost::system::system_error;

App::App() noexcept : ss_shutdown(io_ctx, SIGINT) {

  // set up our shutdown signal handler
  ss_shutdown.async_wait([this](const error_code &ec, int sig) {
    csv fn_id{"ss_shutdown"};

    if (ec) return;

    INFO_AUTO("caught SIGNT({}), requesting stop...", sig);

    thread.request_stop();
  });
}

void App::main() {
  static constexpr csv fn_id{"main"};

  auto main_pid = getpid();

  auto watch = conf::watch(io_ctx);

  Logger::create(io_ctx);

  _logger->info(conf::watch::module_id, "init", watch.msg(conf::ParseMsg::Init));

  Stats::create(io_ctx);

  // Logger is running, start mDNS
  shared::mdns->start();

  std::optional<Rtsp> rtsp;
  auto clock = std::make_unique<MasterClock>(io_ctx);
  auto desk = std::make_unique<Desk>(clock.get());

  try {
    // create Rtsp under try/catch to handle port already in use
    rtsp.emplace(io_ctx, desk.get());

  } catch (const system_error &e) {
    Logger::synchronous();
    rtsp.reset();

    INFO_AUTO("run exception {}", e.what());
  } catch (...) {
    INFO_AUTO("run exception UNKNOWN");
  }

  if (rtsp.has_value()) {
    // rtsp was created successfully, start a thread for App io_ctx
    thread = std::jthread([main_pid, this, &rtsp = *rtsp](std::stop_token stoken) mutable {
      INFO_INIT("sizeof={:>5}", sizeof(App));

      constexpr auto tname{"pierre_app"};
      auto tid = pthread_self();
      if (auto rc = pthread_setname_np(tid, tname); rc != 0) {
        INFO_AUTO("failed to set thread name: {}", std::strerror(errno));
      }

      stop_request_watcher(std::move(stoken));

      io_ctx.run();
    });
  }

  watch.schedule();

  if (thread.joinable()) thread.join();

  pid_file_unlink(conf::fixed::pid_file(), main_pid);

  INFO_AUTO("primary io_ctx has finished all work");

  rtsp.reset();

  shared::mdns.reset();

  Stats::shutdown();
  Logger::shutdown();
}

void App::stop_request_watcher(std::stop_token stoken) noexcept {

  auto sr_timer = std::make_unique<asio::system_timer>(io_ctx, 1s);

  sr_timer->expires_after(1s);
  sr_timer->async_wait([this, stoken = std::move(stoken),
                        sr_timer = std::move(sr_timer)](const error_code &ec) mutable {
    if (ec) return;

    if (stoken.stop_requested()) {
      asio::post(io_ctx, [this]() {
        INFO("stop_request", "detected");

        io_ctx.stop();
      });
    } else {
      stop_request_watcher(std::move(stoken));
    }
  });
}

} // namespace pierre
