/*
    misc/random.hpp - Ruth Common Random Functions
    Copyright (C) 2020  Tim Hughey

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

#ifndef pierre_random_hpp
#define pierre_random_hpp

#include "local/types.hpp"

namespace pierre {

size_t availablePrimes();
uint8_t dieRoll(uint8_t num_sides = 6);
uint8_t diceRoll(uint8_t num_dice = 2, uint8_t num_sides = 6);
bool diceWin(uint8_t match, uint8_t num_dice = 2, uint8_t num_sides = 6);
inline uint8_t roll1D6() { return diceRoll(1, 6); }
inline uint8_t roll2D6() { return diceRoll(2, 6); }
void printDiceRollStats();
uint32_t random(uint32_t modulo);
float randomPercent(uint32_t min = 0, uint32_t max = 100);
uint32_t randomPrime(uint8_t num_primes = 0);

} // namespace pierre

#endif
