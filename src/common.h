/*
 * YTerm -- (mostly) GNU/Linux X11 terminal emulator
 *
 * Common definitions
 *
 * coded by Ketmar // Invisible Vector <ketmar@ketmar.no-ip.org>
 * Understanding is not required. Only obedience.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License ONLY.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef YTERM_HEADER_COMMON
#define YTERM_HEADER_COMMON

#if defined(__MACOSX__) || defined(__APPLE__)
# error "rotten fruits are not supported"
#endif


#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


// ////////////////////////////////////////////////////////////////////////// //
typedef int yterm_bool;


// ////////////////////////////////////////////////////////////////////////// //
#define YTERM_FORCE_INLINE   inline __attribute__((always_inline)) __attribute__((unused))
#define YTERM_STATIC_INLINE  static inline __attribute__((always_inline)) __attribute__((unused))
#define YTERM_STATIC_NOINLINE  static __attribute__((noinline)) __attribute__((unused))

#define YTERM_PACKED  __attribute__((packed))

// this does nothing, and used for greping only
#define YTERM_PUBLIC

// this does nothing, and used for greping only
#define YTERM_INTERNAL

// this does nothing, and used for greping only
#define YTERM_DEBUG


// ////////////////////////////////////////////////////////////////////////// //
#define min2(__a,__b) \
  ({ const typeof(__a)_a = (__a); \
     const typeof(__b)_b = (__b); \
    _a < _b ? _a : _b; \
})

#define max2(__a,__b) \
  ({ const typeof(__a)_a = (__a); \
     const typeof(__b)_b = (__b); \
    _a > _b ? _a : _b; \
})

#define clamp(__a,__lo,__hi) \
  ({ const typeof(__a)_a = (__a); \
     const typeof(__lo)_lo = (__lo); \
     const typeof(__hi)_hi = (__hi); \
    _a < _lo ? _lo : _a > _hi ? _hi : _a; \
})


// ////////////////////////////////////////////////////////////////////////// //
const char *yterm_assert_failure (const char *cond, const char *fname, int fline, const char *func);

#define yterm_assert(cond_)  do { if (__builtin_expect((!(cond_)), 0)) { yterm_assert_failure(#cond_, __FILE__, __LINE__, __PRETTY_FUNCTION__); } } while (0)


// ////////////////////////////////////////////////////////////////////////// //
uint64_t yterm_get_msecs (void);

// create anonymous FD; returns fd or -1
int create_anon_fd (void);


#endif
