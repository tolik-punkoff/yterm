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
/* O_TMPFILE */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

//#define DEBUG_TEMP_FILES

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>


static int yterm_common_temp_counter = 0;


//==========================================================================
//
//  yterm_assert_failure
//
//==========================================================================
const char *yterm_assert_failure (const char *cond, const char *fname, int fline,
                                  const char *func)
{
  for (const char *t = fname; *t; ++t) if (*t == '/') fname = t+1;
  fflush(stdout);
  fprintf(stderr, "\n%s:%d: Assertion in `%s` failed: %s\n", fname, fline, func, cond);
  fflush(stderr);
  abort();
}


// ////////////////////////////////////////////////////////////////////////// //
static time_t secstart = 0;


//==========================================================================
//
//  yterm_get_msecs
//
//==========================================================================
YTERM_PUBLIC uint64_t yterm_get_msecs (void) {
  struct timespec ts;
  #ifdef CLOCK_MONOTONIC
  yterm_assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
  #else
  // this should be available everywhere
  yterm_assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
  #endif
  // first run?
  if (secstart == 0) {
    secstart = ts.tv_sec+1;
    yterm_assert(secstart); // it should not be zero
  }
  return (uint64_t)(ts.tv_sec-secstart+2)*1000U+(uint32_t)ts.tv_nsec/1000000U;
  // nanoseconds
  //return (uint64_t)(ts.tv_sec-secstart+2)*1000000000U+(uint32_t)ts.tv_nsec;
}


//==========================================================================
//
//  create_anon_fd
//
//==========================================================================
YTERM_PUBLIC int create_anon_fd (void) {
  int fd;

  #ifdef DEBUG_TEMP_FILES
  fd = -1;
  #else
    #ifdef O_TMPFILE
    fd = open("/tmp", O_RDWR|O_TMPFILE|O_EXCL, 0644);
    if (fd < 0) {
      fprintf(stderr, "cannot create anon file; errno=%d (%s)\n", errno, strerror(errno));
    }
    #else
    fd = -1;
    #endif
  #endif

  if (fd < 0) {
    // oops, it seems we don't have anonymous files, try a named one
    //fprintf(stderr, "trying named \"anon\" file...\n");
    char buf[128];
    snprintf(buf, sizeof(buf), "/tmp/$k8-yterm$.%d.%d.$$$", (int)getpid(),
             yterm_common_temp_counter);
    yterm_common_temp_counter += 1;
    fd = open(buf, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
      fprintf(stderr, "cannot create hotswap file; errno=%d (%s)\n", errno, strerror(errno));
    } else {
      // delete the file, so it will become anonumous
      #ifdef DEBUG_TEMP_FILES
      fprintf(stderr, "named \"anon\" file: %s\n", buf);
      #else
      unlink(buf);
      #endif
    }
  }

  return (fd >= 0 ? fd : -1);
}
