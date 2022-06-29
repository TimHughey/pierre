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

#pragma once

#include <any>

namespace pierre {
namespace hdu {

/*
Special thanks for the instructive article on polymorphic objects:
https://www.fluentcpp.com/2021/01/29/inheritance-without-pointers/
*/

template <typename Interface> struct Implementation {
public:
  template <typename ConcreteType>
  Implementation(ConcreteType &&object)
      : storage{std::forward<ConcreteType>(object)}, getter{[](std::any &storage) -> Interface & {
          return std::any_cast<ConcreteType &>(storage);
        }} {}

  Interface *operator->() { return &getter(storage); }

private:
  std::any storage;
  Interface &(*getter)(std::any &);
};

} // namespace hdu
} // namespace pierre
