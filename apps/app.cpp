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
#include "git_version.hpp"

#include "lcs/args.hpp"
#include "lcs/config.hpp"
#include "lcs/logger.hpp"
#include "lcs/stats.hpp"
#include "mdns/mdns.hpp"
#include "rtsp/rtsp.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>
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

  auto app = pierre::App();
  auto rc = app.main(argc, argv);

  exit(rc);
}

namespace pierre {

using error_code = boost::system::error_code;

// startup / runtime support

static void check_pid_file(const CliArgs &args) noexcept {
  namespace fs = std::filesystem;

  fs::path file{args.pid_file()};
  bool force{args.force_restart()};

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
      check_pid_file(std::move(args));

    } else if (errno == ESRCH) {
      fs::remove(file);

    } else {
      std::cout << file << "contains" << pid << ":" << std::strerror(errno) << std::endl;
    }
  }
}

static void write_pid_file(const string pid_file) noexcept {
  const auto flags = fmt::file::WRONLY | fmt::file::CREATE | fmt::file::TRUNC;
  auto os = fmt::output_file(pid_file.c_str(), flags);

  os.print("{}", getpid());
  os.close();
}

////
//// App
////

int App::main(int argc, char *argv[]) {
  static constexpr csv fn_id{"main"};

  crypto::init(); // initialize sodium and gcrypt

  // if construction of CliArgs succeds then we are assured command line args
  // are good and help was not requested
  CliArgs cli_args(argc, argv);

  // allocate global Config object
  shared::config = std::make_unique<Config>(cli_args.cli_table, io_ctx);

  if (!config()->parse_msg.empty()) {
    perror(config()->parse_msg.c_str());
    exit(EXIT_FAILURE);
  }

  check_pid_file(cli_args);

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

    write_pid_file(cli_args.pid_file());
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
  INFO_AUTO("{}\n", Config::banner_msg());

  INFO(Config::module_id, "init", "{}\n", config()->init_msg);

  try {
    shared::stats = std::make_unique<Stats>(io_ctx);
    shared::mdns = std::make_unique<mDNS>();

    // create, run then destroy the main application logic
    auto rtsp = std::make_unique<Rtsp>(io_ctx);
    rtsp->run();
    rtsp.reset();

    shared::mdns.reset();
    shared::stats.reset();
    shared::config.reset();
  } catch (const std::exception &e) {

    INFO_AUTO("shutdown caught {}\n", e.what());
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

    INFO_AUTO("caught SIGINT ({})\n", signal, Config::daemon());

    if (Config::daemon()) {
      const auto pid_path = Config::fs_pid_path();

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
      } catch (std::exception &except) {
        INFO_AUTO("{} while removing {}\n", except.what(), pid_path);
      }
    } // end of daemon specific actions

    signal_set_ignore->cancel(); // cancel the remaining work

    io_ctx.stop();
  });
}

} // namespace pierre
