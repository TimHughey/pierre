/*
    Pierre - Custom Light Show via DMX for Wiss Landing
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
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    https://www.wisslanding.com
*/

#include <array>
#include <chrono>
#include <cmath>
#include <ctgmath>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "cli/cli.hpp"
#include "cli/subcmds/dsp.hpp"

using namespace std;
using namespace chrono;

namespace fs = std::filesystem;

namespace pierre {

bool Cli::run() {

  auto tmp_dir = fs::path(fs::temp_directory_path());
  tmp_dir.append("pierre");
  fs::create_directories(tmp_dir);

  recompile_flag.assign(tmp_dir).append("recompile");
  history_file.assign(tmp_dir).append("history");

  linenoiseSetMultiLine(true);
  linenoiseHistorySetMaxLen(250);
  linenoiseHistoryLoad(history_file.c_str());

  repl();

  linenoiseHistorySave(history_file.c_str());

  return true;
}

int Cli::doHelp() const {
  cout << "available commands:" << endl << endl;
  cout << "(h)elp         - display this message" << endl;
  cout << "(l)eave <secs> - leave and shutdown after secs (default: 300)"
       << endl;
  cout << "version        - display git revision and build time" << endl;
  cout << "c              - set recompile flag and immediate shutdown" << endl;
  cout << "q or x         - immediate shutdown" << endl;

  return 0;
}

int Cli::doLeave(const string_t &args) const {
  milliseconds ms = milliseconds(5 * 60 * 1000); // five minutes

  if (args.empty() == false) {
    auto num_secs = std::stoul(args, nullptr, 10);

    if (num_secs > 0) {
      ms = milliseconds(num_secs * 1000);
    }
  }

  core::State::leave(ms);

  return 0;
}

int Cli::doTest(const string_t &args) const {
  int rc = 0;

  if (args.empty()) { // stop the compiler warning...
  }

  cout << "no tests defined" << endl;

  return rc;
}

int Cli::handleLine() {
  int rc = -1;
  string cmd;
  string args;
  auto first_space = input.find_first_of(' ');

  if (input.empty()) {
    return 0;
  }

  if (first_space == string::npos) {
    cmd = input;
  } else {
    cmd = input.substr(0, first_space);
    args = input.substr(first_space + 1, string::npos);
  }

  if ((cmd[0] == 'l') || (cmd.compare("leave") == 0)) {
    rc = doLeave(args);
    goto finished;
  }

  if (cmd.compare("version") == 0) {
    cout << "git revision: " << git_revision;
    cout << " build timestamp: " << build_timestamp << endl;
    rc = 0;

    goto finished;
  }

  if (matchCmd("help", true)) {
    rc = doHelp();
    goto finished;
  }

  if (matchCmd("test", true)) {
    rc = doTest(args);
    goto finished;
  }

  if (cmd.compare("dsp") == 0) {
    auto subcmd = cli::Dsp();
    rc = subcmd.handleCmd(args);

    goto finished;
  }

finished:
  if (rc < 0) {
    cout << "command not found: " << cmd << endl;
  }

  cout << endl;
  cout.flush();

  return rc;
}

bool Cli::matchCmd(const string_t &cmd, bool letter) const {
  auto rc = false;

  if (letter && (input.front() == cmd.front())) {
    rc = true;
  } else if (input.compare(cmd) == 0) {
    rc = true;
  }

  return rc;
}

bool Cli::matchLetter(char letter) const {
  auto rc = false;

  if ((input.length() == 1) && (input[0] == letter)) {
    rc = true;
  }

  return rc;
}

void Cli::repl() {
  uint line_num = 0;

  linenoiseClearScreen();
  cout << "Hello, this is Pierre. " << endl << endl;

  string_t subsys = "pierre";
  char *raw = nullptr;
  while (core::State::running()) {
    array<char, 25> prompt;

    prompt.fill(0x00);

    snprintf(prompt.data(), prompt.size(), "%s [%u] %% ", subsys.c_str(),
             line_num);

    raw = linenoise(prompt.data());

    if (raw == nullptr) {
      continue;
    }

    input = raw;

    if (matchLetter('x') || matchLetter('q')) {
      core::State::quit();
      break;
    }

    if (matchLetter('c')) {
      std::ofstream(recompile_flag) << "";

      core::State::quit();
      break;
    }

    if (handleLine() >= 0) {
      linenoiseHistoryAdd(raw);
    };

    line_num++;

    if (core::State::leaving()) {
      break;
    }

    free(raw);
    raw = nullptr;
  }

  if (raw != nullptr) {
    free(raw);
  }
}

} // namespace pierre
