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
#include "base/crypto.hpp"
#include "base/host.hpp"
#include "base/logger.hpp"
#include "base/stats.hpp"
#include "git_version.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/rtsp.hpp"

#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <exception>
#include <filesystem>
#include <fmt/os.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  auto app = pierre::App(argc, argv);

  // if app is created then we are assured command line args
  // are good, help was not requested and we should proceed to main()
  auto rc = app.main();

  exit(rc);
}

namespace pierre {

namespace fs = std::filesystem;

App::App(int argc, char *argv[]) noexcept
    : cli_args(argc, argv), ctoken(module_id, io_ctx, cli_args.get_table()) {
  crypto::init(); // initialize sodium and gcrypt
}

// startup / runtime support

void App::check_pid_file() noexcept {
  namespace fs = std::filesystem;

  fs::path file{cli_args.pid_file()};
  bool force{cli_args.force_restart()};

  // pid file doesn't exist
  if (!fs::exists(file)) return;

  std::ifstream istrm(file);
  if (istrm.is_open()) {
    int64_t pid;

    istrm >> pid; // read the pid

    auto kill_rc = kill(pid, 0);

    if ((kill_rc == 0) && !force) { // pid is alive and we can signal it
      std::cout << file << "contains live pid " << pid;
      std::cout << ", use --force-restart to restart" << std::endl;

    } else if (kill_rc == 0) {
      kill(pid, SIGINT);
      sleep(1);
      check_pid_file();

    } else if (errno == ESRCH) {
      fs::remove(file);

    } else {
      std::cout << file << "contains" << pid << ":" << std::strerror(errno) << std::endl;
    }
  }
}

void App::write_pid_file() noexcept {
  const auto flags = fmt::file::WRONLY | fmt::file::CREATE | fmt::file::TRUNC;
  auto os = fmt::output_file(cli_args.pid_file().c_str(), flags);

  os.print("{}", getpid());
  os.close();
}

////
//// App
////

int App::main() {
  static constexpr csv fn_id{"main"};

  if (!ctoken.parse_ok()) {
    perror(ctoken.parse_error().c_str());
    exit(EXIT_FAILURE);
  }

  check_pid_file();

  if (cli_args.daemon()) {

    if (syscall(SYS_close_range, 3, ~0U, 0) == -1) {
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
    fs::current_path(fs::path("/"));

    write_pid_file();
  }

  ////
  //// proceed with startup as either a background or foreground process
  ////

  shared::logger = std::make_unique<Logger>();

  signal_set_ignore.emplace(io_ctx, SIGHUP);
  signal_set_shutdown.emplace(io_ctx, SIGINT);

  signals_ignore();   // ignore certain signals
  signals_shutdown(); // catch certain signals for shutdown

  shared::logger->async(io_ctx);
  // INFO_AUTO("{}\n", Config::banner_msg());

  INFO(conf::token::module_id, "init", "{}\n", ctoken.get_init_msg());

  try {
    shared::stats = std::make_unique<Stats>(io_ctx);
    shared::mdns = std::make_unique<mDNS>();

    // create, run then destroy the main application logic
    auto rtsp = std::make_unique<Rtsp>(io_ctx);
    rtsp->run();
    rtsp.reset();

    shared::mdns.reset();
    shared::stats.reset();
  } catch (const std::exception &e) {
    INFO_AUTO("run exception {}\n", e.what());
  }

  return 0;
}

void App::signals_ignore() noexcept {
  static constexpr csv fn_id{"sig_ignore"};

  signal_set_ignore->async_wait([this](const error_code ec, int signal) {
    if (!ec) {
      signals_ignore();

      INFO_AUTO("caught SIGHUP ({})\n", signal);
    }
  });
}

void App::signals_shutdown() noexcept {
  static constexpr csv fn_id{"shutdown"};

  signal_set_shutdown->async_wait([this](const error_code ec, int signal) {
    if (ec) return;

    INFO_AUTO("caught SIGINT ({})\n", signal, cli_args.daemon());

    if (cli_args.daemon()) {
      const fs::path pid_path{cli_args.pid_file()};

      try {
        std::ifstream ifs(pid_path);
        int64_t stored_pid;

        ifs >> stored_pid;

        if (stored_pid == getpid()) {
          std::filesystem::remove(pid_path);
          INFO_AUTO("unlinking {}\n", pid_path);
        } else {
          INFO_AUTO("saved pid({}) does not match process pid({})\n", stored_pid, getpid());
        }
      } catch (std::exception &e) {
        INFO_AUTO("{} while removing {}\n", e.what(), pid_path);
      }
    } // end of daemon specific actions

    signal_set_ignore->cancel(); // cancel the remaining work

    io_ctx.stop();
  });
}

} // namespace pierre
