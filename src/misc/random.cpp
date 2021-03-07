/*
    misc/random.cpp
    Ruth Common Random Functions

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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "misc/random.hpp"

namespace pierre {

static uint64_t _roll_count = 0;
static uint64_t _dice_counts[25] = {};

// initialize the array of primes
static uint32_t _primes[] = {
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,
    53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113,
    127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197,
    199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277};

static uint64_t _random() {
  int fd;
  uint64_t num;

  fd = open("/dev/random", O_RDONLY);
  read(fd, &num, sizeof(uint64_t));

  close(fd);

  return num;
}

size_t availablePrimes() { return sizeof(_primes) / sizeof(uint32_t); }

uint8_t dieRoll(uint8_t num_sides) {
  const uint8_t die = (_random() % num_sides) + 1;

  return die;
}

uint8_t diceRoll(uint8_t num_dice, uint8_t num_sides) {
  uint8_t sum = 0;
  for (auto i = 0; i < num_dice; i++) {
    sum += dieRoll(num_sides);
  }

  _roll_count++;
  _dice_counts[sum]++;

  return sum;
}

bool diceWin(uint8_t match, uint8_t num_dice, uint8_t num_sides) {
  bool win = false;
  if (diceRoll(num_dice, num_sides) == match) {
    win = true;
  }

  return win;
}

void printDiceRollStats() {
  if (_roll_count > 0) {
    printf("\n  2d6    percent    roll count\n");
    printf("  ---    -------    ----------\n");

    for (auto i = 2; i < 25; i++) {
      const double dice_count = static_cast<double>(_dice_counts[i]);
      const double roll_count = static_cast<double>(_roll_count);

      const double percent = (dice_count / roll_count) * 100.0;

      if (percent > 0.0) {
        printf("  %3d    %5.2f%%   %12lu\n", i, percent, _dice_counts[i]);
      }
    }

    printf("\n  total rolls: %lu\n\n", _roll_count);
  } else {
    printf("\nzero die rolls\n\n");
  }
}

uint32_t random(uint32_t modulo) { return (_random() % modulo) + 1; }

float randomPercent(const uint32_t min, const uint32_t max) {
  const uint32_t ceiling = max - min;

  const float val =
      (static_cast<float>(random(ceiling)) + static_cast<float>(min)) / 100.0f;

  return val;
}

uint32_t randomPrime(uint8_t num_primes) {
  uint8_t index; // reminder, next must be >=0 and < availablePrimes()

  if ((num_primes == 0) || (num_primes > availablePrimes())) {
    index = random(availablePrimes()) - 1;
  } else {
    index = random(num_primes) - 1;
  }

  return _primes[index];
}

} // namespace pierre
