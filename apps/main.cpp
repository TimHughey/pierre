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

#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "core/state.hpp"
#include "pierre.hpp"

using namespace std;
using namespace pierre;
using namespace boost::program_options;
namespace fs = std::filesystem;
namespace po = boost::program_options;
using string = std::string;

int findConfigFile(fs::path &cfg_file);
int setupWorkingDirectory();
bool parseArgs(int ac, char *av[]);

// global variable to contain cmd line args
po::variables_map vm;
constexpr const char *dmx_host = "dmx-host";
constexpr const char *colorbars = "colorbars";

int main(int argc, char *argv[]) {

  if ((argc > 1) && !parseArgs(argc, argv)) {
    exit(-1024);
  }

  fs::path cfg_file;
  auto find_rc = findConfigFile(cfg_file);
  if (find_rc != 0) {
    exit(find_rc);
  };

  error_code ec;

  ec = State::initConfig()->parse(cfg_file);

  if (ec) {
    cerr << "config file parse failed: " << ec.message() << endl;
    exit(-1000);
  }

  auto wd_rc = setupWorkingDirectory();

  if (wd_rc != 0) {
    exit(wd_rc);
  }

  if (vm.count(colorbars)) {
    auto desk = State::config("lightdesk"sv);

    desk->insert_or_assign("colorbars"sv, true);
  }

  if (vm.count(dmx_host)) {
    toml::table *dmx = State::config("dmx"sv);

    dmx->insert_or_assign("host"sv, vm[dmx_host].as<std::string>());
  }

  // auto pid = getpid();
  // setpriority(PRIO_PROCESS, pid, -10);

  auto pierre = make_shared<Pierre>();

  pierre->run();

  return 0;
}

int findConfigFile(fs::path &cfg_file) {
  bool rc = -1022; // not found after searching paths

  auto home_env = getenv("HOME");

  if (home_env == nullptr) {
    cerr << "environment variable HOME is unset, aborting" << endl;
    return -1;
  }

  // command line specified config files are always used if found.
  // if not found application is terminated.
  auto env_cfg = getenv("PIERRE_CFG_FILE");
  if (env_cfg) {
    fs::path env_cfg_path(env_cfg);

    error_code ec;
    if (fs::exists(env_cfg_path, ec)) {
      cfg_file = env_cfg_path;
      return 0;
    } else {
      cerr << "config " << env_cfg_path << ": " << ec.message() << endl;
      return -1023;
    }
  }

  // build a list of search directories for the config file
  vector<fs::path> search_dirs;
  search_dirs.emplace_back(fs::path(home_env).append(".pierre"));
  search_dirs.emplace_back(fs::path("/usr/local/etc/pierre"));
  search_dirs.emplace_back(fs::path("/etc/pierre"));

  // search for the config file
  for (const auto &p : search_dirs) {
    fs::path f = fs::path(p).append("live.toml");

    error_code ec;
    if (fs::exists(f, ec)) {
      cfg_file = f;
      rc = 0;
      break;
    }
  }

  if (rc != 0) {
    cerr << "could not find a configuration file" << endl;
  }

  return rc;
}

bool parseArgs(int ac, char *av[]) {
  const char *cfg_file = "cfg-file";
  const char *cfg_file_help = "config file (overrides env PIERRE_CFG_FILE)";

  try {
    // Declare the supported options.
    po::options_description desc("pierre options:");
    desc.add_options()("help", "display this help text")(
        cfg_file, po::value<string>(), cfg_file_help)(
        dmx_host, po::value<string>(), "stream dmx frames to host")(
        colorbars, "pinspot color bar test at startup");

    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      cout << desc << "\n";
      return false;
    }
  } catch (const error &ex) {
    cerr << ex.what() << endl;
  }

  return true;
}

int setupWorkingDirectory() {
  auto base = State::config()->table()["pierre"sv];
  fs::path dir;

  if (base["use_home"sv].value_or(false)) {
    dir.append(getenv("HOME"));
  }

  dir.append(base["working_dir"sv].value_or("/tmp"));

  error_code ec;
  fs::current_path(dir, ec);

  if (ec) {
    cerr << "unable to set working directory: " << ec.message() << endl;
    return -1021;
  }

  return 0;
}
