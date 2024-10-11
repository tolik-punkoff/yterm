/*
 * YTerm -- (mostly) GNU/Linux X11 terminal emulator
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
/*
  communication with clipboard manager.
  included directly into the main file.
*/

//#define DEBUG_PASTE

#define PASTE_BUF_SIZE  (4096)

static char paste_buf[PASTE_BUF_SIZE];
static char paste_buf2[PASTE_BUF_SIZE];


//==========================================================================
//
//  xsel_wait
//
//==========================================================================
static void xsel_wait (pid_t child) {
  //fprintf(stderr, "waiting...\n");
  const uint64_t stt = yterm_get_msecs();
  for (;;) {
    int wstatus;
    if (waitpid(child, &wstatus, 0) == child) break;
    uint64_t ctt = yterm_get_msecs();
    if (ctt - stt > 6000) {
      kill(child, SIGKILL);
    }
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 50 * 1000000;
    nanosleep(&ts, NULL);
  }
  //fprintf(stderr, "done.\n");
}


//==========================================================================
//
//  xxread
//
//==========================================================================
static int xxread (int fd, void *buf, int size) {
  yterm_assert(size > 0);
  for (;;) {
    ssize_t rd = read(fd, buf, (size_t)size);
    if (rd < 0) {
      if (errno == EINTR) continue;
      rd = -1;
    }
    return rd;
  }
}


//==========================================================================
//
//  do_paste
//
//==========================================================================
static void do_paste (Term *term, int mode) {
  if (term == NULL || term->deadstate != DS_ALIVE || term->child.cmdfd < 0) return;
  if (mode < 0 || mode > 2) return;
  if (opt_paste_from[mode] == NULL || opt_paste_from[mode][0] == 0) return;

  #ifdef DEBUG_PASTE
  fprintf(stderr, "do_paste: mode=%d: %s\n", mode, opt_paste_from[mode]);
  #endif

  ExecData ed;
  if (execsh_prepare(&ed, opt_paste_from[mode]) == 0) return;

  int nullfd = open("/dev/null", O_RDONLY);
  if (nullfd < 0) {
    execsh_free(&ed);
    fprintf(stderr, "ERROR: cannot open \"/dev/null\"!\n");
    return;
  }

  // [0] is reading end (read from), [1] is writing end (write to)
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    close(nullfd);
    execsh_free(&ed);
    fprintf(stderr, "ERROR: cannot create pipe!\n");
    return;
  }

  // just in case
  //sigblock(SIGCHLD);

  /* done in the `main()`
  sigset_t sset;
  sigemptyset(&sset);
  sigaddset(&sset, SIGCHLD);
  sigprocmask(SIG_BLOCK, &sset, NULL);
  sigemptyset(&sset);
  */

  pid_t child = fork();
  if (child == -1) {
    close(nullfd);
    close(pipefd[0]);
    close(pipefd[1]);
    execsh_free(&ed);
    fprintf(stderr, "ERROR: cannot fork for 'xsel'\n");
    return;
  }

  if (child == 0) {
    // child
    setsid();

    close(pipefd[0]); // close reading (from the parent) end
    dup2(nullfd, STDIN_FILENO); // make stdin read from /dev/null
    dup2(nullfd, STDERR_FILENO); // make stderr write to /dev/null
    dup2(pipefd[1], STDOUT_FILENO); // redirect output to parent
    close(nullfd);
    close(pipefd[1]); // we cloned the fd, so the original one is not needed anymore

    execsh(&ed);
  }

  execsh_free(&ed);

  // master
  close(nullfd);
  close(pipefd[1]); // close writing end

  one_term_write(term);

  yterm_bool bp_sent = 0;
  yterm_bool last_was_cr = 0;
  yterm_bool first = 1;
  ssize_t rd = xxread(pipefd[0], paste_buf, (size_t)PASTE_BUF_SIZE);
  #ifdef DEBUG_PASTE
  uint32_t total = 0;
  #endif
  while (rd > 0) {
    #ifdef DEBUG_PASTE
    total += (unsigned)rd;
    fprintf(stderr, "..read: %d; total: %u\n", (int)rd, total);
    #endif
    // sanitize "cr, lf" and lone "cr"s
    ssize_t spos = 0;
    ssize_t dpos = 0;
    while (spos != rd) {
      const char ch = paste_buf[spos]; spos += 1;
      if (ch == '\n') {
        // convert LF to CR, as xterm does (i believe)
        if (!last_was_cr && !first) {
          paste_buf2[dpos] = '\r'; dpos += 1;
        }
      } else if (ch == '\t') {
        // keep tabs only for bracketed paste mode
        if ((term->mode & YTERM_MODE_BRACPASTE) != 0) {
          paste_buf2[dpos] = '\t';
        } else {
          paste_buf2[dpos] = ' ';
        }
        dpos += 1;
        first = 0;
      } else if (ch < 0 || (ch >= 32 && ch != 127)) {
        paste_buf2[dpos] = ch; dpos += 1;
        first = 0;
      }
      last_was_cr = (ch == '\r');
    }

    #ifdef DEBUG_PASTE
    fprintf(stderr, "...converted: %u of %u\n", dpos, spos);
    #endif
    // read more
    rd = xxread(pipefd[0], paste_buf, (size_t)PASTE_BUF_SIZE);
    if (rd <= 0) {
      //FIXME: this "protection" doesn't work properly
      while (dpos != 0 && paste_buf2[dpos - 1] >= 0 && paste_buf2[dpos - 1] <= 32) dpos -= 1;
    }

    #ifdef DEBUG_PASTE
    fprintf(stderr, "...new-read: %d bytes\n", (int)rd);
    #endif

    if (dpos != 0) {
      if (bp_sent == 0 && (term->mode & YTERM_MODE_BRACPASTE) != 0) {
        //fprintf(stderr, "BPC!\n");
        bp_sent = 1;
        term_write(term, "\x1b[200~");
      }
      term_write_sanitize(term, paste_buf2, dpos);
    }
  }
  close(pipefd[0]);

  if (bp_sent != 0) term_write(term, "\x1b[201~");

  xsel_wait(child);
}


//==========================================================================
//
//  xxwrite
//
//==========================================================================
static yterm_bool xxwrite (int fd, const void *buf, size_t count) {
  const char *cbuf = (const char *)buf;
  while (count != 0) {
    ssize_t wr = write(fd, cbuf, count);
    if (wr < 0 && errno == EINTR) continue;
    if (wr < 0) return 0;
    count -= (size_t)wr;
    cbuf += (size_t)wr;
  }
  return 1;
}


//==========================================================================
//
//  do_copy_to
//
//==========================================================================
static void do_copy_to (Term *term, int mode) {
  if (term == NULL || !TERM_IN_SELECTION(term)) return;
  term_selection_end(term);
  if (mode < 0 || mode > 2) return;
  if (!term_has_selblock(term)) return;
  if (opt_copy_to[mode] == NULL || opt_copy_to[mode][0] == 0) return;

  #if 0
  fprintf(stderr, "do_copy_to: mode=%d: %s\n", mode, opt_copy_to[mode]);
  #endif

  ExecData ed;
  if (execsh_prepare(&ed, opt_copy_to[mode]) == 0) return;

  int nullfd = open("/dev/null", O_RDONLY);
  if (nullfd < 0) {
    execsh_free(&ed);
    fprintf(stderr, "ERROR: cannot open \"/dev/null\"!\n");
    return;
  }

  // [0] is reading end (read from), [1] is writing end (write to)
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    close(nullfd);
    execsh_free(&ed);
    fprintf(stderr, "ERROR: cannot create pipe!\n");
    return;
  }

  // just in case
  //sigblock(SIGCHLD);

  /* done in the `main()`
  sigset_t sset;
  sigemptyset(&sset);
  sigaddset(&sset, SIGCHLD);
  sigprocmask(SIG_BLOCK, &sset, NULL);
  sigemptyset(&sset);
  */

  pid_t child = fork();
  if (child == -1) {
    close(nullfd);
    close(pipefd[0]);
    close(pipefd[1]);
    execsh_free(&ed);
    fprintf(stderr, "ERROR: cannot fork for 'xsel'\n");
    return;
  }

  if (child == 0) {
    // child
    setsid();

    close(pipefd[1]); // close writing (from the parent) end
    dup2(pipefd[0], STDIN_FILENO); // make stdin read from /dev/null
    dup2(nullfd, STDERR_FILENO); // make stderr write to /dev/null
    dup2(nullfd, STDOUT_FILENO); // redirect output to parent
    close(nullfd);
    close(pipefd[0]); // we cloned the fd, so the original one is not needed anymore

    execsh(&ed);
  }

  execsh_free(&ed);

  // master
  close(nullfd);
  close(pipefd[0]); // close reading end

  char *ubuf = malloc((unsigned)hvbuf.width * 4 + 16);
  if (ubuf != NULL) {
    #if 0
    fprintf(stderr, "=== from %d to %d (lc: %d)\n", term->history.y0, term->history.y1,
            term->history.file.lcount);
    #endif
    for (int y = term->history.y0; y <= term->history.y1; y += 1) {
      const CharCell *line = history_load_line(TERM_CBUF(term), y, &term->history.file);
      if (line == NULL) {
        #if 0
        fprintf(stderr, "y: %d -- WTF?!\n", y);
        #endif
        continue;
      }

      int x0 = 0, x1 = hvbuf.width - 1;
      switch (term->history.blocktype) {
        case SBLOCK_STREAM:
          if (y == term->history.y0) x0 = term->history.x0;
          if (y == term->history.y1) x1 = term->history.x1;
          break;
        case SBLOCK_LINE:
          break;
        case SBLOCK_RECT:
          x0 = term->history.x0;
          x1 = term->history.x1;
          break;
      }

      // do we need a newline?
      yterm_bool write_nl;
      if (y != term->history.y1) {
        switch (term->history.blocktype) {
          case SBLOCK_STREAM:
            write_nl = ((line[hvbuf.width - 1].flags & CELL_AUTO_WRAP) == 0);
            break;
          case SBLOCK_LINE:
          case SBLOCK_RECT:
            write_nl = 1;
            break;
          default:
            fprintf(stderr, "SHIT! ketmar forgot some block type! (0514)\n");
            write_nl = 1;
            break;
        }
      } else {
        write_nl = 0;
      }

      // convert to utf8
      uint32_t dpos = 0;
      for (int x = x0; x <= x1; x += 1) {
        uint32_t cp = line[x].ch;
        if (cp == 0) cp = 32;
        dpos += yterm_utf8_encode(ubuf + dpos, cp);
      }

      if (term->history.blocktype != SBLOCK_RECT) {
        if (write_nl || y == term->history.y1) {
          while (dpos > 0 && ubuf[dpos - 1] >= 0 && ubuf[dpos - 1] <= 32) dpos -= 1;
        }
      }

      #if 0
      ubuf[dpos] = 0;
      fprintf(stderr, "y=%d; x0=%d; x1=%d; nl=%d\n  <%s>\n", y, x0, x1, write_nl, ubuf);
      #endif

      if (xxwrite(pipefd[1], ubuf, dpos) == 0) break;

      if (write_nl) {
        ubuf[0] = '\n';
        if (xxwrite(pipefd[1], ubuf, 1) == 0) break;
      }
    }

    free(ubuf);
  }

  close(pipefd[1]);

  xsel_wait(child);
}
