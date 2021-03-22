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
#include <iostream>

#include "cli/cli.hpp"

using namespace std;
using namespace chrono;

namespace pierre {
Cli::Cli() {}

bool Cli::run() {
  repl();

  return true;
}

int Cli::doHelp() const {
  cout << "available commands:" << endl << endl;
  cout << "(h)elp       - display this message" << endl;
  cout << "(l)eave <secs> - leave and shutdown after secs (default: 300)"
       << endl;
  cout << "version      - display git revision and build time" << endl;
  cout << "q or x       - immediately shutdown" << endl;

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

int Cli::handleLine(const string_t line) {
  int rc = -1;
  string cmd;
  string args;
  auto first_space = line.find_first_of(' ');

  if (line.empty()) {
    return 0;
  }

  if (first_space == string::npos) {
    cmd = line;
  } else {
    cmd = line.substr(0, first_space);
    args = line.substr(first_space + 1, string::npos);
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

  if ((cmd[0] == 'h') || (cmd.compare("help") == 0)) {
    rc = doHelp();
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

void Cli::repl() {
  uint line_num = 0;

  linenoiseSetMultiLine(true);

  linenoiseClearScreen();
  cout << "Hello, this is Pierre. " << endl << endl;

  while (core::State::running()) {
    array<char, 25> prompt;
    char *input;

    prompt.fill(0x00);

    snprintf(prompt.data(), prompt.size(), "pierre [%u] %% ", line_num);

    input = linenoise(prompt.data());

    if (input == nullptr) {
      continue;
    }

    if ((input[0] == 'x') || input[0] == 'q') {
      core::State::quit();
      break;
    }

    handleLine(input);
    free(input);
    line_num++;

    if (core::State::leaving()) {
      break;
    }
  }
}

} // namespace pierre
