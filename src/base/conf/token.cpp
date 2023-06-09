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

#define TOML_IMPLEMENTATION

#include "conf/token.hpp"
#include "base/logger.hpp"
#include "base/uuid.hpp"
#include "conf/cli_args.hpp"
#include "conf/fixed.hpp"
#include "conf/parser.hpp"
#include "conf/watch.hpp"

#include <filesystem>
#include <fmt/format.h>

namespace pierre {
namespace conf {

token::token(csv mid) noexcept : uuid(UUID()), root{mid} {
  parse(ttable, msgs);

  root_node = ttable.at_path(root);
}

token::token(csv mid, watch *watcher) noexcept : uuid(UUID()), root(mid), watcher(watcher) {
  parse(ttable, msgs);
}

token::~token() noexcept {}

bool token::latest() noexcept {
  INFO_AUTO_CAT("latest");

  auto table_fut = watcher->latest();

  auto rc = table_fut.valid();

  if (rc) {
    ttable = std::move(table_fut.get());
    root_node = ttable.at_path(root);
    INFO_AUTO("{}", *this);
  } else {
    INFO_AUTO("invalid future {}", *this);
  }

  return rc;
}

token *token::acquire_watch_token(csv mid) noexcept { return watch::self->make_token(mid); }

void token::initiate_watch() noexcept {
  if (watcher) watcher->initiate_watch(*this);
}

void token::release() const noexcept {
  if (watcher != nullptr) watcher->release_token(this);
}

} // namespace conf
} // namespace pierre
