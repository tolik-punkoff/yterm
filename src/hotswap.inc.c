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
  binary hotswap support.
  included directly into the main file.
*/

#define HOTSWAP_SIGNATURE  "!K8-YTERM-HOTSWAP-FILE!VERSION!0!"
#define HOTSWAP_CLI_ARG    "--HOTSWAP!--HOTSWAP?--HOTSWAP!--"


static int hotswap_version = 0;

enum {
  HOTSWAP_READ = 0,
  HOTSWAP_WRITE = 1,
};


//==========================================================================
//
//  mark_all_fds
//
//==========================================================================
static void mark_all_fds (yterm_bool ascloexec) {
  for (Term *term = termlist; term != NULL; term = term->next) {
    if (term->child.cmdfd >= 0) {
      int flags = fcntl(term->child.cmdfd, F_GETFD, NULL);
      if (flags != -1) {
        if (ascloexec) flags |= FD_CLOEXEC; else flags &= ~FD_CLOEXEC;
        if (fcntl(term->child.cmdfd, F_SETFD, flags) == -1) {
          fprintf(stderr, "HOTSWAP: OOPS, cannot set master fd (%d) flags.\n", term->child.cmdfd);
        }
      } else {
        fprintf(stderr, "HOTSWAP: OOPS, cannot read master fd (%d) flags.\n", term->child.cmdfd);
      }
    }
  }
}


//==========================================================================
//
//  hotswap_io_chunk
//
//==========================================================================
static yterm_bool hotswap_io_chunk (void *ptr, size_t size, yterm_bool do_write) {
  yterm_bool res;
  if (do_write) {
    res = (size == 0 || write(opt_debug_hotswap_fd, ptr, size) == (ssize_t)size);
  } else {
    res = (size == 0 || read(opt_debug_hotswap_fd, ptr, size) == (ssize_t)size);
  }
  return res;
}


//==========================================================================
//
//  hotswap_io_str
//
//==========================================================================
static yterm_bool hotswap_io_str (char **str, yterm_bool do_write) {
  yterm_bool res = 0;
  uint16_t slen;
  if (do_write) {
    const size_t xlen = (*str != NULL ? strlen(*str) : 0);
    yterm_assert(xlen <= 0xffffU);
    slen = (uint16_t)xlen;
    if (write(opt_debug_hotswap_fd, &slen, sizeof(slen)) == (ssize_t)sizeof(slen)) {
      if (slen == 0 || write(opt_debug_hotswap_fd, *str, slen) == (ssize_t)slen) {
        res = 1;
      }
    }
  } else {
    if (read(opt_debug_hotswap_fd, &slen, sizeof(slen)) == (ssize_t)sizeof(slen)) {
      free(*str); *str = calloc(1, slen + 1);
      if (*str != NULL) {
        if (slen == 0 || read(opt_debug_hotswap_fd, *str, slen) == (ssize_t)slen) {
          res = 1;
        }
      }
    }
  }
  return res;
}


#define HS_IO_BUF(var_)  \
  if (!hotswap_io_chunk(&(var_), sizeof(var_), do_write)) return 0

#define HS_IO_STR(var_)  \
  if (!hotswap_io_str(&(var_), do_write)) exit(1)


//==========================================================================
//
//  hotswap_start_reading
//
//==========================================================================
static yterm_bool hotswap_signature (yterm_bool do_write) {
  const int fd = opt_debug_hotswap_fd;
  yterm_bool res = 0;

  if (fd >= 0) {
    char sign[33];
    if (do_write) {
      memcpy(sign, HOTSWAP_SIGNATURE, 33);
      res = (write(fd, sign, sizeof(sign)) == (ssize_t)sizeof(sign));
    } else {
      if (lseek(fd, 0, SEEK_SET) != (off_t)-1) {
        if (read(fd, sign, sizeof(sign)) == (ssize_t)sizeof(sign)) {
          res = (memcmp(sign, HOTSWAP_SIGNATURE, 33) == 0);
        }
      }
    }
  }

  return res;
}


//==========================================================================
//
//  hotswap_io_init
//
//  load some initial options
//
//==========================================================================
static yterm_bool hotswap_io_init (yterm_bool do_write) {
  if (!hotswap_signature(do_write)) return 0;

  fprintf(stderr, "HOTSWAP: %s options...\n", (do_write ? "writing" : "reading"));

  if (do_write) {
    hotswap_version = 1;
  }
  HS_IO_BUF(hotswap_version);
  if (hotswap_version != 0 && hotswap_version != 1) return 0;

  HS_IO_STR(xrm_app_name);
  HS_IO_STR(opt_term); // $TERM value
  HS_IO_STR(opt_class);
  HS_IO_STR(opt_title);
  HS_IO_BUF(opt_term_type);
  HS_IO_BUF(opt_enable_tabs);
  HS_IO_BUF(opt_esc_twice);
  HS_IO_BUF(opt_mouse_reports);
  HS_IO_BUF(opt_cur_blink_time);
  HS_IO_BUF(opt_tabs_visible);
  HS_IO_BUF(opt_history_enabled);
  if (hotswap_version == 1) {
    HS_IO_BUF(x11_embed);
  }

  if (!do_write) {
    // seal some important hotswap options
    xrm_seal_option_by_ptr(&xrm_app_name);
    xrm_seal_option_by_ptr(&opt_term);
    xrm_seal_option_by_ptr(&opt_class);
    xrm_seal_option_by_ptr(&opt_title);
    xrm_seal_option_by_ptr(&opt_term_type);
    xrm_seal_option_by_ptr(&opt_enable_tabs);
    xrm_seal_option_by_ptr(&opt_tabs_visible);
    //xrm_seal_option_by_ptr(&opt_history_enabled);
  }

  return 1;
}


typedef struct YTERM_PACKED {
  uint32_t ch; // unicrap char
  uint32_t flags;
  uint32_t fg; // rgb color
  uint32_t bg; // rgb color
} CharCellOld;


//==========================================================================
//
//  hotswap_io_cbuf
//
//==========================================================================
static yterm_bool hotswap_io_cbuf (void *ptr, int tw, int th, yterm_bool do_write) {
  yterm_assert(ptr != NULL);
  yterm_assert(tw > 0 && th > 0);
  // sadly, cbuf format can change
  CharCellOld cold;
  CharCell *cc = (CharCell *)ptr;
  for (int f = tw * th; f != 0; f -= 1, cc += 1) {
    if (do_write) {
      // write
      cold.ch = cc->ch;
      cold.flags = cc->flags;
      cold.fg = cc->fg;
      cold.bg = cc->bg;
    }
    HS_IO_BUF(cold.ch);
    HS_IO_BUF(cold.flags);
    HS_IO_BUF(cold.fg);
    HS_IO_BUF(cold.bg);
    if (!do_write) {
      // read
      cc->ch = cold.ch;
      cc->flags = cold.flags;
      cc->fg = cold.fg;
      cc->bg = cold.bg;
    }
  }
  return 1;
}




//==========================================================================
//
//  hotswap_io_one_tab
//
//==========================================================================
static yterm_bool hotswap_io_one_tab (Term *term, yterm_bool do_write) {
  uint8_t bt;
  // size
  int tw, th;
  if (do_write) {
    // writing
    tw = term->main.cbuf.width;
    th = term->main.cbuf.height;
  }
  HS_IO_BUF(tw);
  HS_IO_BUF(th);
  // is alt buffer active?
  if (term->alt.cbuf.width != 0) bt = 37; else bt = 93;
  HS_IO_BUF(bt);
  if (bt != 37 && bt != 93) return 0;
  if (!do_write) {
    // reading
    yterm_assert(tw > 0 && tw <= MaxBufferWidth);
    yterm_assert(th > 0 && th <= MaxBufferHeight);
    cbuf_resize(&term->main.cbuf, tw, th, 0);
    if (bt == 37) {
      cbuf_resize(&term->alt.cbuf, tw, th, 0);
    } else {
      cbuf_free(&term->alt.cbuf);
    }
  } else {
    // writing
    if (term->alt.cbuf.width == 0) {
      yterm_assert(term->alt.cbuf.height == 0);
    } else {
      yterm_assert(term->alt.cbuf.width == term->main.cbuf.width);
      yterm_assert(term->alt.cbuf.height == term->main.cbuf.height);
    }
  }
  //const size_t cbsz = (unsigned)(tw * th) * sizeof(CharCell);
  HS_IO_BUF(term->active);
  HS_IO_BUF(term->mode);
  HS_IO_BUF(term->charset);
  HS_IO_BUF(term->escseq);
  HS_IO_BUF(term->child);
  HS_IO_BUF(term->title);
  HS_IO_BUF(term->deadstate);
  HS_IO_BUF(term->wrbuf.size);
  HS_IO_BUF(term->wrbuf.used);
  // output buffer
  if (!do_write) {
    // reading, prepare the buffer
    free(term->wrbuf.buf); term->wrbuf.buf = NULL;
    if (term->wrbuf.used != 0) {
      yterm_assert(term->wrbuf.used <= term->wrbuf.size);
      term->wrbuf.buf = malloc(term->wrbuf.size);
      if (term->wrbuf.buf == NULL) abort();
    } else {
      term->wrbuf.size = 0;
    }
  }
  if (!hotswap_io_chunk(term->wrbuf.buf, term->wrbuf.used, do_write)) return 0;
  HS_IO_BUF(term->rdcp);
  HS_IO_BUF(term->mreport);
  //HS_IO_BUF(term->tab); // no need to write tab info, it will be recreated
  HS_IO_BUF(term->colorMode);
  HS_IO_BUF(term->mouseReports);
  HS_IO_BUF(term->escesc);
  HS_IO_BUF(term->history.enabled);
  // main screen
  HS_IO_BUF(term->main.curpos);
  HS_IO_BUF(term->main.cursaved);
  HS_IO_BUF(term->main.currattr);
  HS_IO_BUF(term->main.curhidden);
  HS_IO_BUF(term->main.scTop);
  HS_IO_BUF(term->main.scBot);
  HS_IO_BUF(term->main.attrsaved);
  HS_IO_BUF(term->main.charsetsaved);
  HS_IO_BUF(term->main.modesaved);
  // main buffer contents
  HS_IO_BUF(term->main.cbuf.dirtyCount);
  if (!hotswap_io_cbuf(term->main.cbuf.buf, tw, th, do_write)) return 0;
  // alt screen
  HS_IO_BUF(term->alt.curpos);
  HS_IO_BUF(term->alt.cursaved);
  HS_IO_BUF(term->alt.currattr);
  HS_IO_BUF(term->alt.curhidden);
  HS_IO_BUF(term->alt.scTop);
  HS_IO_BUF(term->alt.scBot);
  HS_IO_BUF(term->alt.attrsaved);
  HS_IO_BUF(term->alt.charsetsaved);
  HS_IO_BUF(term->alt.modesaved);
  // alt buffer contents
  HS_IO_BUF(term->alt.cbuf.dirtyCount);
  if (term->alt.cbuf.width != 0) {
    if (!hotswap_io_cbuf(term->alt.cbuf.buf, tw, th, do_write)) return 0;
  } else {
    yterm_assert(term->alt.cbuf.dirtyCount == 0);
  }
  // which screen is active?
  if (term->wkscr == &term->main) bt = 33; else bt = 77;
  HS_IO_BUF(bt);
  if (bt != 33 && bt != 77) return 0;
  if (!do_write) {
    if (bt == 33) {
      term->wkscr = &term->main;
    } else {
      yterm_assert(term->alt.cbuf.width != 0);
      term->wkscr = &term->alt;
    }
  }
  return 1;
}


//==========================================================================
//
//  hotswap_load_options
//
//==========================================================================
static void hotswap_load_options (void) {
  if (!hotswap_io_init(HOTSWAP_READ)) exit(1);
}


//==========================================================================
//
//  hotswap_load_tabs
//
//==========================================================================
static void hotswap_load_tabs (void) {
  yterm_assert(opt_debug_hotswap_fd >= 0);

  fprintf(stderr, "HOTSWAP: reading tabs...\n");

  Term *acc = NULL;

  // now read/write structs directly
  uint8_t more;
  for (;;) {
    more = 42;
    if (!hotswap_io_chunk(&more, sizeof(more), HOTSWAP_READ)) exit(1);
    if (more == 96) break;
    if (more != 42) exit(1);
    // create and load new tab
    Term *term = term_create_new_term();
    if (term == NULL) exit(1);
    if (!hotswap_io_one_tab(term, HOTSWAP_READ)) exit(1);
    // remember active tab
    if (term->active) acc = term;
    // mark dirty
    cbuf_mark_all_dirty(&term->main.cbuf);
    cbuf_mark_all_dirty(&term->alt.cbuf);
    fprintf(stderr, "HOTSWAP: read tab: cmdfd=%d; pid=%d\n",
            term->child.cmdfd, (int)term->child.pid);
  }

  close(opt_debug_hotswap_fd);
  opt_debug_hotswap_fd = -1;

  mark_all_fds(1);

  currterm = NULL;
  if (acc) {
    currterm = acc;
    x11_change_title(currterm->title.last);
  }

  force_frame_redraw(1);
  force_tab_redraw();

  fprintf(stderr, "HOTSWAP: loading finished.\n");
}


//==========================================================================
//
//  exec_hotswap
//
//==========================================================================
static void exec_hotswap (void) {
  int fd = create_anon_fd();

  fprintf(stderr, "HOTSWAP: binary is <%s>\n", exe_path);

  opt_debug_hotswap_fd = fd;

  if (!hotswap_io_init(HOTSWAP_WRITE)) goto error;

  fprintf(stderr, "HOTSWAP: writing tabs...\n");
  uint8_t more;
  for (Term *term = termlist; term != NULL; term = term->next) {
    more = 42;
    if (!hotswap_io_chunk(&more, sizeof(more), HOTSWAP_WRITE)) goto error;
    fprintf(stderr, "HOTSWAP: writing tab: cmdfd=%d; pid=%d\n",
            term->child.cmdfd, (int)term->child.pid);
    if (!hotswap_io_one_tab(term, HOTSWAP_WRITE)) goto error;
  }
  // no more terminals
  more = 96;
  if (!hotswap_io_chunk(&more, sizeof(more), HOTSWAP_WRITE)) goto error;

  mark_all_fds(0);

  XDestroyWindow(x11_dpy, x11_win);
  XCloseDisplay(x11_dpy);

  // now execute self
  fprintf(stderr, "HOTSWAP: executing...\n");
  static char *exe_main_args[4];
  exe_main_args[0] = exe_path;
  exe_main_args[1] = HOTSWAP_CLI_ARG;
  char *xbuf = malloc(256);
  yterm_assert(xbuf != NULL);
  snprintf(xbuf, 256, "%d", opt_debug_hotswap_fd);
  exe_main_args[2] = xbuf;
  exe_main_args[3] = NULL;
  execvp(exe_main_args[0], exe_main_args);

  fprintf(stderr, "HOTSWAP: shit, it failed! errno=%d : %s\n",
          errno, strerror(errno));
  exit(1);

error:
  close(fd);
  opt_debug_hotswap_fd = -1;
  fprintf(stderr, "HOTSWAP: error writing swap file!\n");
  return;
}
