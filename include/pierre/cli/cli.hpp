/*
    Cli - Custom Light Show via DMX for Wiss Landing
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

#ifndef _pierre_cli_hpp
#define _pierre_cli_hpp

#include <filesystem>
#include <memory>
#include <thread>

#include "cli/linenoise.h"
#include "cli/subcmds/subcmd.hpp"
#include "core/state.hpp"
#include "local/types.hpp"
#include "version.h"

namespace pierre {

class Cli {
public:
  Cli() = default;

  Cli(const Cli &) = delete;
  Cli &operator=(const Cli &) = delete;

  bool run();

private:
  int doHelp() const;
  int doLeave(const string_t &args) const;
  int doTest(const string_t &args) const;
  int handleLine();
  void repl();
  bool matchLetter(char letter) const;
  bool matchCmd(const string_t &cmd, bool letter = false) const;

private:
  string_t input;

  const string_t git_revision = GIT_REVISION;
  const string_t build_timestamp = BUILD_TIMESTAMP;
  std::filesystem::path recompile_flag;
  std::filesystem::path history_file;

  std::vector<cli::upSubCmd> _subcmds;
};

} // namespace pierre

#endif
