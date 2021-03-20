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

void Cli::repl() {
  uint line_num = 0;
  array<char, 25> prompt;
  prompt.fill(0x00);

  linenoiseSetMultiLine(true);

  cout << endl << endl;
  cout << "Hello, this is Pierre." << endl;

  while (_shutdown == false) {
    char *input;
    string_t line;

    snprintf(prompt.data(), prompt.size(), "pierre [%u] %% ", line_num);

    input = linenoise(prompt.data());

    if (input == nullptr) {
      continue;
    }

    if ((input[0] == 0x03) || (input[0] == 0x04) || input[0] == 'q') {
      return;
    }

    line = input;
    free(input);

    auto first_space = line.find_first_of(' ');

    if (first_space == string::npos) {
      continue;
    }

    auto cmd = line.substr(0, first_space);
    auto args = line.substr(first_space + 1, string::npos);

    cout << "cmd[" << cmd << "] args[" << args << "]" << endl;
  }
}

} // namespace pierre
