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
  various terminal procedures.
  included directly into the main file.
*/

#define HAS_DIRTY(tt_)  \
  ((tt_) != NULL \
    ? (TERM_IN_SELECTION(tt_) ? hvbuf.dirtyCount : TERM_CBUF(tt_)->dirtyCount) \
    : 0)

#define CALL_RENDER(code_)  do { \
  const yterm_bool oldgi = cbufInverse; \
  const int oldcm = colorMode; \
  CursorPos *rcpos; \
  CellBuffer *rcbuf; \
  CursorPos scpos; \
  Region clipreg = x11_render_osd_menus(curr_menu); \
  if (term->osd_messages != NULL) { \
    cbufInverse = 0; \
    colorMode = CMODE_NORMAL; \
    clipreg = x11_render_osd(term->osd_messages->message, clipreg); \
  } \
  if (!TERM_IN_SELECTION(term)) { \
    rcpos = (TERM_CURVISIBLE(term) ? TERM_CPOS(term) : NULL); \
    rcbuf = TERM_CBUF(term); \
    cbufInverse = !!(term->mode & YTERM_MODE_REVERSE); \
    if (term->colorMode >= 0) colorMode = term->colorMode; \
  } else { \
    cbufInverse = 0; \
    colorMode = (opt_amber_tint ? CMODE_AMBER_TINT : CMODE_AMBER); \
    rcpos = &scpos; \
    rcbuf = &hvbuf; \
    scpos.x = term->history.cx; \
    scpos.y = term->history.cy; \
  } \
  XSetRegion(x11_dpy, x11_gc, clipreg); \
  XDestroyRegion(clipreg); /* we don't need it anymore */ \
  code_ \
  /*if (term->osd_messages != NULL || curr_menu != NULL)*/ { \
    /* reset OSD clip mask */ \
    XSetClipMask(x11_dpy, x11_gc, None); \
  } \
  cbufInverse = oldgi; \
  colorMode = oldcm; \
} while (0)


//==========================================================================
//
//  term_send_size_ioctl
//
//==========================================================================
static void term_send_size_ioctl (Term *term) {
  if (term != NULL && term->child.cmdfd >= 0) {
    struct winsize w;
    w.ws_row = TERM_CBUF(term)->height;
    w.ws_col = TERM_CBUF(term)->width;
    w.ws_xpixel = 0;
    w.ws_ypixel = 0;
    if (ioctl(term->child.cmdfd, TIOCSWINSZ, &w) < 0) {
      fprintf(stderr, "WARNING: couldn't set window size: %s\n", strerror(errno));
    }
    #if 0
    else {
      fprintf(stderr, "WINCH! nw=%d; nh=%d\n",
              TERM_CBUF(term)->height, TERM_CBUF(term)->width);
    }
    #endif
  }
}


//==========================================================================
//
//  term_reset_mode
//
//==========================================================================
static void term_reset_mode (Term *term) {
  if (term != NULL) {
    term->wkscr = &term->main; //k8: dunno

    term->escseq.state = 0;
    term->mode = YTERM_MODE_WRAP/*|YTERM_MODE_MOUSEBTN*//*|YTERM_MODE_GFX1*/;
    term->mreport.mode = 1000;
    term->mreport.lastx = -1;
    term->mreport.lasty = -1;
    term->charset = YTERM_MODE_GFX0;

    term->main.curpos.x = 0; term->main.curpos.y = 0;
    term->main.curpos.lastColFlag = 0;
    term->main.curpos.justMoved = 0;
    term->main.cursaved = term->main.curpos;
    term->main.curhidden = 0;
    term->main.currattr.ch = 0x20;
    term->main.currattr.flags = 0;
    term->main.currattr.fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
    term->main.currattr.bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
    term->main.scTop = 0; term->main.scBot = term->main.cbuf.height - 1;
    term->main.attrsaved = term->main.currattr;
    term->main.charsetsaved = 0;
    term->main.modesaved = 0;

    term->alt.curpos = term->main.curpos;
    term->alt.cursaved = term->alt.curpos;
    term->alt.currattr = term->main.currattr;
    term->alt.curhidden = 0;
    term->alt.scTop = 0; term->alt.scBot = term->alt.cbuf.height - 1;
    term->alt.attrsaved = term->alt.currattr;
    term->alt.charsetsaved = 0;
    term->alt.modesaved = 0;
    // free alt buffer; it will be recreated if necessary
    cbuf_free(&term->alt.cbuf);
  }
}


//==========================================================================
//
//  term_init
//
//  initialise term structure, create screens, etc.
//
//==========================================================================
static void term_init (Term *term, int w, int h) {
  yterm_assert(term != NULL);
  yterm_assert(w >= MinBufferWidth && w <= MaxBufferWidth);
  yterm_assert(h >= MinBufferHeight && h <= MaxBufferHeight);

  memset(term, 0, sizeof(Term));

  cbuf_new(&term->main.cbuf, w, h);
  //cbuf_new(&term->alt.cbuf, w, h);
  term->wkscr = &term->main;

  memset(&term->history.file, 0, sizeof(term->history.file));
  term->history.file.fd = -1;
  term->history.enabled = opt_history_enabled;

  term->active = 0;
  term->colorMode = -1;
  term->mouseReports = -1;
  term->escesc = -1;
  term->reportMods = 0;
  term->lastModKeys = 0;
  term->savedValues = 0;

  term->child.pid = 0;
  term->child.cmdfd = -1;

  term->title.pgrp = 0;
  snprintf(term->title.last, sizeof(term->title.last), "%s", opt_title);
  term->title.custom = 0;
  term->title.next_check_time = 0;

  term->deadstate = DS_ALIVE;

  term->wrbuf.buf = NULL;
  term->wrbuf.size = 0;
  term->wrbuf.used = 0;

  term->rdcp = 0;

  term->last_acttime = 0;

  term_reset_mode(term);

  term->prev = NULL;
  term->next = NULL;
}


//==========================================================================
//
//  term_wipe_osd_messages
//
//==========================================================================
static void term_wipe_osd_messages (Term *term) {
  if (term != NULL) {
    while (term->osd_messages != NULL) {
      Message *msg = term->osd_messages;
      term->osd_messages = msg->next;
      free(msg->message);
    }
  }
}


//==========================================================================
//
//  term_cleanup
//
//  this WIPES the whole struct
//
//==========================================================================
static void term_cleanup (Term *term) {
  if (term) {
    cbuf_free(&term->main.cbuf);
    cbuf_free(&term->alt.cbuf);
    free(term->wrbuf.buf);
    if (term->history.file.fd > 2) {
      history_close(&term->history.file);
    }
    memset(term, 0, sizeof(Term));
    term->history.file.fd = -1;
    term->child.cmdfd = -1;
    term_wipe_osd_messages(term);
  }
}


//==========================================================================
//
//  term_close_fd
//
//==========================================================================
static void term_close_fd (Term *term) {
  if (term != NULL) {
    if (term->child.cmdfd >= 0) {
      close(term->child.cmdfd);
      term->child.cmdfd = -1;
    }
    free(term->wrbuf.buf); term->wrbuf.buf = NULL;
    term->wrbuf.size = 0; term->wrbuf.used = 0;
  }
}


//==========================================================================
//
//  term_kill_child
//
//  kill child, close fd, free output buffer
//
//==========================================================================
static void term_kill_child (Term *term) {
  if (term != NULL) {
    if (term->child.pid != 0) {
      kill(term->child.pid, SIGKILL);
      term->child.pid = 0;
    }
    term_close_fd(term);
  }
}


//==========================================================================
//
//  term_osd_message
//
//==========================================================================
static void term_osd_message (Term *term, const char *text) {
  if (term != NULL && term->active && term->deadstate != DS_DEAD &&
      text != NULL && text[0] != 0)
  {
    const int timeout = 3000;

    if (term->osd_messages != NULL) {
      // invalidate top line (that's where OSDs are)
      cbuf_mark_line_dirty(TERM_CBUF(term), 0);
      term_wipe_osd_messages(term);
    }

    Message *msg = calloc(1, sizeof(Message));
    if (msg == NULL) return;
    msg->message = strdup(text);
    if (msg->message == NULL) { free(msg); return; }
    msg->next = NULL;

    Message *last = term->osd_messages;
    if (last == NULL) {
      msg->time = yterm_get_msecs() + (unsigned)timeout;
      term->osd_messages = msg;
    } else {
      msg->time = (unsigned)timeout;
      while (last->next != NULL) last = last->next;
      last->next = msg;
    }
  }
}


//==========================================================================
//
//  term_invalidate_cursor
//
//==========================================================================
YTERM_STATIC_INLINE void term_invalidate_cursor (Term *term) {
  if (term != NULL && TERM_CURVISIBLE(term)) {
    const CursorPos *cpos = TERM_CPOS(term);
    cbuf_mark_dirty(TERM_CBUF(term), cpos->x, cpos->y); // this will erase the old cursor
  }
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  SaveSimple = 0,
  SaveDEC = 1,
  SaveANSI = 2, // same as DEC for now
  SaveXTerm = 3,
};


//==========================================================================
//
//  term_save_cursor
//
//==========================================================================
static void term_save_cursor (Term *term, int fullmode) {
  term->wkscr->cursaved = *TERM_CPOS(term);
  if (fullmode != SaveSimple) {
    term->wkscr->attrsaved = term->wkscr->currattr;
    term->wkscr->charsetsaved = term->charset;
    term->wkscr->modesaved = term->mode;
    // also save autowrapping
    if (fullmode == SaveXTerm) {
      term->wkscr->modesaved |= (term->mode & YTERM_MODE_WRAP);
    }
  }
}


//==========================================================================
//
//  term_restore_cursor
//
//==========================================================================
static void term_restore_cursor (Term *term, int fullmode) {
  const int ox = TERM_CPOS(term)->x;
  const int oy = TERM_CPOS(term)->y;
  const int dorest = (term->wkscr->cursaved.x != ox || term->wkscr->cursaved.y != oy);
  if (dorest) term_invalidate_cursor(term);
  *TERM_CPOS(term) = term->wkscr->cursaved;
  if (dorest) term_invalidate_cursor(term);
  // restore other attrs
  if (fullmode != SaveSimple) {
    uint32_t mask = YTERM_MODE_GFX_MASK;
    // also restore autowrapping
    if (fullmode == SaveXTerm) mask |= YTERM_MODE_WRAP;
    term->wkscr->currattr = term->wkscr->attrsaved;
    term->mode = (term->mode & ~mask) | (term->wkscr->modesaved & mask);
    term->charset = (term->charset & ~YTERM_MODE_GFX_MASK) |
                    (term->wkscr->charsetsaved & YTERM_MODE_GFX_MASK);
  }
}


//==========================================================================
//
//  sign
//
//==========================================================================
YTERM_STATIC_INLINE int sign (int v) {
  return (v < 0 ? -1 : v > 0 ? +1 : 0);
}


//==========================================================================
//
//  term_scroll_internal
//
//  negative is up
//
//==========================================================================
static void term_scroll_internal (Term *term, int lines) {
  if (term == NULL || lines == 0) return;

  CellBuffer *cbuf = TERM_CBUF(term);
  const int wdt = cbuf->width;
  const int stop = TERM_SCTOP(term);
  const int sbot = TERM_SCBOT(term);

  const int dir = (lines < 0 ? -1 : +1);
  const int regh = sbot - stop + 1;
  lines = abs(lines); // it is safe, there will never be any overflow
  if (lines >= regh) {
    // the whole area is scrolled, just clear it
    if (term->scroll_count != 0 && !term_scroll_locked(term)) {
      // mark previous scroll region as dirty, why not
      cbuf_mark_region_dirty(cbuf, 0, term->scroll_y0, cbuf->width - 1, term->scroll_y1);
    }
    cbuf_clear_region(cbuf, 0, stop, wdt - 1, sbot, TERM_CATTR(term));
    term_lock_scroll(term);
  } else {
    // erase the cursor (because why not)
    term_invalidate_cursor(term);

    // if the window is invisible, required actions are taken by X event processor
    yterm_bool do_copy = winVisible && term->active && !term_scroll_locked(term) &&
                         // for now, don't bother with scoll acceleration, if OSD is active
                         curr_menu == NULL && term->osd_messages == NULL;

    // can accumulate more scroll?
    if (do_copy) {
      if (term->scroll_count == 0) {
        // first scroll, remember the area
        term->scroll_y0 = stop;
        term->scroll_y1 = sbot;
        term->scroll_count = dir * lines;
      } else if (sign(term->scroll_count) != dir || abs(term->scroll_count) + lines >= regh ||
                 term->scroll_y0 != stop || term->scroll_y1 != sbot)
      {
        // the whole region is scrolled away, or different direction, or different scroll area
        // dirty only previously scrolled area
        cbuf_mark_region_dirty(cbuf, 0, term->scroll_y0, cbuf->width - 1, term->scroll_y1);
        term_lock_scroll(term);
        do_copy = 0;
      } else {
        // accumulate
        yterm_assert(sign(term->scroll_count) == dir);
        term->scroll_count += dir * lines;
      }
    } else if (term->scroll_count != 0 && !term_scroll_locked(term)) {
      // cannot copy anymore: dirty previously scrolled area
      cbuf_mark_region_dirty(cbuf, 0, term->scroll_y0, cbuf->width - 1, term->scroll_y1);
      term_lock_scroll(term);
    }

    // scroll it
    if (dir < 0) {
      // up
      cbuf_scroll_area_up(cbuf, stop, sbot, lines,
                          (do_copy ? CBUF_DIRTY_OVERWRITE : CBUF_DIRTY_MERGE),
                          TERM_CATTR(term));
    } else {
      // down
      cbuf_scroll_area_down(cbuf, stop, sbot, lines,
                            (do_copy ? CBUF_DIRTY_OVERWRITE : CBUF_DIRTY_MERGE),
                            TERM_CATTR(term));
    }

    // put the cursor back (because why not)
    term_invalidate_cursor(term);
  }
}


//==========================================================================
//
//  term_scroll_up
//
//  called either for normal scroll, or for automatic scroll
//
//==========================================================================
YTERM_STATIC_INLINE void term_scroll_up (Term *term, int lines) {
  if (term != NULL && lines > 0) {
    // write to history?
    if (lines == 1 && term->history.enabled && term->wkscr == &term->main &&
        term->wkscr->scTop == 0)
    {
      // top line will be scrolled away, save it to history file
      history_append_line(TERM_CBUF(term), 0, &term->history.file);
    }
    term_scroll_internal(term, -lines);
  }
}


//==========================================================================
//
//  term_scroll_down
//
//==========================================================================
YTERM_STATIC_INLINE void term_scroll_down (Term *term, int lines) {
  if (term != NULL && lines > 0) {
    term_scroll_internal(term, lines);
  }
}


//==========================================================================
//
//  term_lcf_do
//
//==========================================================================
YTERM_STATIC_INLINE void term_lcf_do (Term *term) {
  CursorPos *cpos = TERM_CPOS(term);
  if (cpos->lastColFlag != 0) {
    yterm_assert(cpos->x == TERM_CBUF(term)->width - 1);
    cpos->lastColFlag = 0;
    cpos->justMoved = 0;
    // mark this line as autowrapped
    cbuf_mark_line_autowrap(TERM_CBUF(term), cpos->y);
    // move curosor to the next line
    term_invalidate_cursor(term);
    cpos->x = 0;
    if (cpos->y == TERM_SCBOT(term)) {
      term_scroll_up(term, 1);
    } else if (cpos->y != TERM_CBUF(term)->height - 1) {
      cpos->y += 1;
      term_invalidate_cursor(term);
    }
  }
}


//==========================================================================
//
//  term_lcf_reset
//
//==========================================================================
YTERM_STATIC_INLINE void term_lcf_reset (Term *term) {
  TERM_CPOS(term)->lastColFlag = 0;
  TERM_CPOS(term)->justMoved = 1;
}


//==========================================================================
//
//  term_lcf_reset_nocmove
//
//==========================================================================
YTERM_STATIC_INLINE void term_lcf_reset_nocmove (Term *term) {
  TERM_CPOS(term)->lastColFlag = 0;
}


//==========================================================================
//
//  term_curmove_abs
//
//==========================================================================
static void term_curmove_abs (Term *term, int nx, int ny) {
  if (term != NULL) {
    term_lcf_reset(term);
    CursorPos *cpos = TERM_CPOS(term);
    nx = clamp(nx, 0, TERM_CBUF(term)->width - 1);
    if ((term->mode & YTERM_MODE_ORIGIN) != 0) {
      ny = clamp(ny, 0, TERM_CBUF(term)->height - 1); // avoid overflows
      ny = min2(ny + TERM_SCTOP(term), TERM_SCBOT(term));
    } else {
      ny = clamp(ny, 0, TERM_CBUF(term)->height - 1);
    }
    if (nx != cpos->x || ny != cpos->y) {
      term_invalidate_cursor(term);
      cpos->x = nx; cpos->y = ny;
      cpos->justMoved = 1;
      term_invalidate_cursor(term);
    }
  }
}


//==========================================================================
//
//  term_curmove_rel
//
//==========================================================================
static void term_curmove_rel (Term *term, int dx, int dy) {
  if (term != NULL) {
    term_lcf_reset(term);
    CursorPos *cpos = TERM_CPOS(term);
    dx = clamp(dx, -32767, 32767);
    dy = clamp(dy, -32767, 32767);
    int nx = clamp(cpos->x + dx, 0, TERM_CBUF(term)->width - 1);
    int ny = cpos->y;
    if ((term->mode & YTERM_MODE_ORIGIN) != 0) {
      // i believe that it is first clamped to the region, and then moved
      // it may be totally wrong, thoug
      ny = clamp(ny, TERM_SCTOP(term), TERM_SCBOT(term));
      ny = clamp(ny + dy, TERM_SCTOP(term), TERM_SCBOT(term));
    } else {
      ny = clamp(ny + dy, 0, TERM_CBUF(term)->height - 1);
    }
    if (nx != cpos->x || ny != cpos->y) {
      term_invalidate_cursor(term);
      cpos->x = nx; cpos->y = ny;
      cpos->justMoved = 1;
      term_invalidate_cursor(term);
    }
  }
}


static const uint16_t term_font_gfx_table[32] = {
  0x25c6, 0x2592, 0x0062, 0x0063, 0x0064, 0x0065, 0x00b0, 0x00b1, // 0x60..0x67
  0x0068, 0x0069, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0x006f, // 0x68..0x6f
  0x0070, 0x2500, 0x0072, 0x0073, 0x251c, 0x2524, 0x2534, 0x252c, // 0x70..0x77
  0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7, 0x0020, // 0x78..0x7f
};


//==========================================================================
//
//  term_putc
//
//  write unicode char to terminal, advance cursor
//  correcly processes some control chars (BS, CR, LF, TAB)
//
//==========================================================================
static void term_putc (Term *term, uint32_t cp) {
  if (term == NULL) return;

  CursorPos *cpos = TERM_CPOS(term);
  CellBuffer *cbuf = TERM_CBUF(term);

  if (cp >= 0x20) {
    // normal char
    if ((term->mode & term->charset) != 0 && cp >= 0x60 && cp < 0x7f) {
      // gfx charset
      if (opt_terminus_gfx != 0) {
        cp -= 0x5f;
      } else {
        cp = term_font_gfx_table[cp - 0x60];
      }
    }
    if (cp == 0x7f) return; //k8: dunno
    if (cpos->justMoved) {
      cpos->justMoved = 0;
      cbuf_unmark_line_autowrap(cbuf, cpos->y);
    }
    // scroll, and write
    //const yterm_bool oldlcf = TERM_CPOS(term)->lastColFlag;
    //if (oldlcf) term_erase_cursor(term);
    term_lcf_do(term);
    // insert mode?
    if ((term->mode & YTERM_MODE_INSERT) != 0) {
      cbuf_insert_chars(cbuf, cpos->x, cpos->y, 1, NULL);
    }
    cbuf_write_wchar(cbuf, cpos->x, cpos->y, cp, TERM_CATTR(term));
    // advance cursor
    if ((term->mode & YTERM_MODE_WRAP) != 0) {
      // wrapping enabled
      if (cpos->x == cbuf->width - 1) {
        yterm_assert(cpos->lastColFlag == 0);
        cpos->lastColFlag = 1;
        cbuf_unmark_line_autowrap(cbuf, cpos->y);
      } else {
        term_invalidate_cursor(term);
        cpos->x += 1;
        term_invalidate_cursor(term);
      }
    } else {
      // wrapping disabled
      if (cpos->x != cbuf->width - 1) {
        term_invalidate_cursor(term);
        cpos->x += 1;
        term_invalidate_cursor(term);
      } else {
        cbuf_unmark_line_autowrap(cbuf, cpos->y);
      }
    }
    //if (oldlcf) term_draw_cursor(term);
  } else {
    int nx;
    // control char
    switch (cp) {
      case 0x07: // BELL
        if (term->active) {
          // do not spam with bells
          uint64_t ctt = yterm_get_msecs();
          if (ctt - lastBellTime >= 100) {
            lastBellTime = ctt;
            x11_bell();
            if (!winFocused || !winVisible) {
              x11_set_urgency(1);
            }
          }
        }
        break;
      case 0x08: // BS
        /*
        if (cpos->lastColFlag != 0) {
          cpos->lastColFlag = 0;
          term_lcf_reset(term); // indicate cursor movement
        } else
        */
        {
          term_lcf_reset(term);
          if (cpos->x != 0)
          if (cpos->x != 0 || cpos->y != 0) {
            term_invalidate_cursor(term);
            if (cpos->x == 0) {
              cpos->x = cbuf->width - 1;
              cpos->y -= 1;
            } else {
              cpos->x -= 1;
            }
            term_invalidate_cursor(term);
          }
        }
        break;
      case 0x09: // TAB
        // i'm pretty sure that it works like this
        // at least, this is what makes vttest happy ;-)
        term_lcf_reset(term);
        nx = min2((cpos->x | 7) + 1, cbuf->width - 1);
        if (nx != cpos->x) {
          term_invalidate_cursor(term);
          cpos->x = nx;
          term_invalidate_cursor(term);
        }
        break;
      case 0x0A: // LF
      case 0x0B: // VT
      case 0x0C: // FF
        term_lcf_reset(term);
        cbuf_unmark_line_autowrap(cbuf, cpos->y);
        if (cpos->y == TERM_SCBOT(term)) {
          // at the bottom of the scroll area, just scroll
          term_scroll_up(term, 1);
          if ((term->mode & YTERM_MODE_CRLF) != 0 && cpos->x != 0) {
            cpos->x = 0;
            term_invalidate_cursor(term);
          }
        } else {
          term_invalidate_cursor(term);
          if ((term->mode & YTERM_MODE_CRLF) != 0) cpos->x = 0;
          if (cpos->y != cbuf->height - 1) cpos->y += 1;
          term_invalidate_cursor(term);
        }
        break;
      case 0x0D: // CR
        term_lcf_reset(term);
        cbuf_unmark_line_autowrap(cbuf, cpos->y);
        if (cpos->x != 0) {
          term_invalidate_cursor(term);
          cpos->x = 0;
          term_invalidate_cursor(term);
        }
        break;
      case 0x1A: // SUB: according to "wraptest", this should clear the flag too
        term_lcf_reset_nocmove(term);
        break;
    }
  }
}


//==========================================================================
//
//  term_draw_info_window
//
//  draw utf-8 info window with the given message
//  used to notify about sudden processe death
//
//==========================================================================
static void term_draw_info_window (Term *term, const char *msg) {
  if (msg == NULL || msg[0] == 0) return;

  // this will hide the cursor
  term_invalidate_cursor(term);

  // reset terminal attributes
  term->escseq.state = 0;
  term->mode = 0/*YTERM_MODE_WRAP*/; // do not wrap text
  term->charset = YTERM_MODE_GFX0;

  const int wdt = TERM_CBUF(term)->width - 2;

  TERM_CPOS(term)->x = 0; TERM_CPOS(term)->y = 0;
  TERM_CPOS(term)->lastColFlag = 0;
  term->wkscr->curhidden = 1;
  TERM_CATTR(term)->flags = 0;
  TERM_SCTOP(term) = 0; TERM_SCBOT(term) = TERM_CBUF(term)->height - 1;


  // draw top window frame
  TERM_CATTR(term)->fg = 0xffffff;
  TERM_CATTR(term)->bg = 0xaa0000;
  cbuf_write_wchar(TERM_CBUF(term), 0, 0, 0x259b, TERM_CATTR(term));
  cbuf_write_wchar_count(TERM_CBUF(term), 1, 0, 0x2580, wdt, TERM_CATTR(term));
  cbuf_write_wchar(TERM_CBUF(term), wdt + 1, 0, 0x259c, TERM_CATTR(term));

  // draw window contents
  TERM_CATTR(term)->fg = 0xaa0000;
  TERM_CATTR(term)->bg = 0xffffff;
  cbuf_write_wchar(TERM_CBUF(term), 0, 1, 0x2590, TERM_CATTR(term));
  TERM_CATTR(term)->fg = 0xffffff;
  TERM_CATTR(term)->bg = 0xaa0000;
  cbuf_write_wchar_count(TERM_CBUF(term), 1, 1, ' ', wdt, TERM_CATTR(term));
  cbuf_write_wchar(TERM_CBUF(term), wdt + 1, 1, 0x2590, TERM_CATTR(term));

  // draw bottom window frame
  cbuf_write_wchar(TERM_CBUF(term), 0, 2, 0x2599, TERM_CATTR(term));
  TERM_CATTR(term)->fg = 0xaa0000;
  TERM_CATTR(term)->bg = 0xffffff;
  cbuf_write_wchar_count(TERM_CBUF(term), 1, 2, 0x2580, wdt, TERM_CATTR(term));
  TERM_CATTR(term)->fg = 0xffffff;
  TERM_CATTR(term)->bg = 0xaa0000;
  cbuf_write_wchar(TERM_CBUF(term), wdt + 1, 2, 0x259f, TERM_CATTR(term));

  TERM_CATTR(term)->fg = 0xffff00;
  uint32_t cp = 0;
  int x = 1;
  while (*msg && x < wdt) {
    cp = yterm_utf8d_consume(cp, *msg); msg += 1;
    if (yterm_utf8_valid_cp(cp)) {
      cbuf_write_wchar(TERM_CBUF(term), x, 1, cp, TERM_CATTR(term));
      x += 1;
    }
  }
}


//==========================================================================
//
//  term_fix_size
//
//==========================================================================
static yterm_bool term_fix_size (Term *term) {
  if (term == NULL) return 0;

  const int neww = clamp(winWidth / charWidth, 1, MaxBufferWidth);
  const int newh = clamp(winHeight / charHeight, 1, MaxBufferHeight);
  #if 0
  fprintf(stderr, "RSZ: oldw=%d; oldh=%d; neww=%d; newh=%d; v=%d; m=%d\n",
          cbuf->width, cbuf->height, neww, newh,
          winVisible, winMapped);
  #endif

  if (neww != TERM_CBUF(term)->width || newh != TERM_CBUF(term)->height) {
    #if 0
    fprintf(stderr, "NEW TERM SIZE: %dx%d\n", neww, newh);
    #endif

    // reset selection mode for this terminal
    term->history.blocktype = SBLOCK_NONE;
    if (term->active) cbuf_free(&hvbuf);

    term->scroll_count = 0;

    // resize main screen
    CursorPos *cpos = &term->main.curpos;
    yterm_bool atbot = cpos->y >= term->main.cbuf.width;
    if (cpos->x >= neww) cpos->x = neww - 1;
    if (atbot) cpos->y = newh - 1; else if (cpos->y >= newh) cpos->y = newh - 1;
    cbuf_resize(&term->main.cbuf, neww, newh, 1); /* relayout it */
    // reset scroll area
    term->main.scTop = 0; term->main.scBot = newh - 1;

    // resize alt screen
    cpos = &term->alt.curpos;
    atbot = cpos->y >= term->alt.cbuf.width;
    if (cpos->x >= neww) cpos->x = neww - 1;
    if (atbot) cpos->y = newh - 1; else if (cpos->y >= newh) cpos->y = newh - 1;
    if (term->alt.cbuf.width != 0 || term->wkscr == &term->alt) {
      cbuf_resize(&term->alt.cbuf, neww, newh, 0); /* do not relayout it */
    } else {
      cbuf_free(&term->alt.cbuf);
    }
    // reset scroll area
    term->alt.scTop = 0; term->alt.scBot = newh - 1;

    term->main.curpos.lastColFlag = 0;
    term->main.curpos.justMoved = 0;

    term->alt.curpos.lastColFlag = 0;
    term->alt.curpos.justMoved = 0;

    term_send_size_ioctl(term);

    #if 0
    fprintf(stderr, "NEW REAL TERM SIZE: %dx%d\n",
            term->wkscr->cbuf.width, term->wkscr->cbuf.height);
    #endif

    return 1;
  }

  return 0;
}


//==========================================================================
//
//  term_can_read
//
//==========================================================================
static yterm_bool term_can_read (Term *term) {
  if (term != NULL && term->deadstate == DS_ALIVE && term->child.cmdfd >= 0) {
    fd_set rfd;
    for (;;) {
      struct timeval timeout = {0};
      FD_ZERO(&rfd);
      FD_SET(term->child.cmdfd, &rfd);
      const int sres = select(term->child.cmdfd + 1, &rfd, NULL, NULL, &timeout);
      if (sres < 0) {
        if (errno == EINTR) continue;
        //k8t_die("select failed: %s", strerror(errno));
        return 0;
      } else {
        return (sres > 0 /*&& FD_ISSET(term->cmdfd, &rfd)*/ ? 1 : 0);
      }
    }
  } else {
    return 0;
  }
}


//==========================================================================
//
//  term_can_write
//
//==========================================================================
static yterm_bool term_can_write (Term *term) {
  if (term != NULL && term->deadstate == DS_ALIVE && term->child.cmdfd >= 0) {
    fd_set wfd;
    for (;;) {
      struct timeval timeout = {0};
      FD_ZERO(&wfd);
      FD_SET(term->child.cmdfd, &wfd);
      const int sres = select(term->child.cmdfd + 1, NULL, &wfd, NULL, &timeout);
      if (sres < 0) {
        if (errno == EINTR) continue;
        return 0;
      } else {
        return (sres > 0 /*&& FD_ISSET(term->child.cmdfd, &wfd)*/ ? 1 : 0);
      }
    }
  } else {
    return 0;
  }
}


//==========================================================================
//
//  term_write_raw
//
//==========================================================================
static void term_write_raw (Term *term, const void *buf, int buflen) {
  if (buf != NULL && buflen != 0 && term != NULL &&
      term->deadstate == DS_ALIVE && term->child.cmdfd >= 0)
  {
    if (buflen < 0) buflen = (int)strlen((const char *)buf);
    if (term->wrbuf.used + (uint32_t)buflen > term->wrbuf.size) {
      // grow buffer
      if (term->wrbuf.size >= 0x00200000U ||
          0x00200000U - term->wrbuf.size < (uint32_t)buflen)
      {
        fprintf(stderr, "ERROR: terminal output buffer overflow!\n");
        term_close_fd(term);
        return;
      }
      uint32_t newsz;
      if (term->wrbuf.used + (uint32_t)buflen <= WRITE_BUF_SIZE) {
        newsz = WRITE_BUF_SIZE;
      } else {
        newsz = ((term->wrbuf.used + (uint32_t)buflen) | 4095) + 1;
      }
      char *nbuf = realloc(term->wrbuf.buf, newsz);
      if (nbuf == NULL) {
        fprintf(stderr, "ERROR: out of memory for terminal output buffer!\n");
        term_close_fd(term);
        return;
      }
      #ifdef DUMP_WRITE_GROW
      fprintf(stderr, "GROWING: %u -> %u (used: %u)\n",
              term->wrbuf.size, newsz, term->wrbuf.used);
      #endif
      term->wrbuf.buf = nbuf;
      term->wrbuf.size = newsz;
    }
    memcpy(term->wrbuf.buf + term->wrbuf.used, buf, (uint32_t)buflen);
    term->wrbuf.used += (uint32_t)buflen;
  }
  if (term->deadstate != DS_ALIVE) {
    term_close_fd(term);
  }
}


//==========================================================================
//
//  term_write
//
//==========================================================================
static void term_write (Term *term, const char *str) {
  if (str && str[0] && term != NULL && term->deadstate == DS_ALIVE && term->child.cmdfd >= 0) {
    if (koiLocale) {
      // utf -> koi
      const char *end = str;
      while (*end > 0 && *end < 127) end += 1;
      // write what we have
      if (end != str) {
        term_write_raw(term, str, (int)(ptrdiff_t)(end - str));
        str = end;
      }
      // decode rest
      char dcbuf[64];
      int dcpos = 0;
      uint32_t cp = 0;
      while (*str) {
        cp = yterm_utf8d_consume(cp, *str); str += 1;
        if (yterm_utf8_valid_cp(cp)) {
          char koi = yterm_uni2koi(cp);
          if (koi) {
            if (dcpos == (int)sizeof(dcbuf)) {
              term_write_raw(term, dcbuf, dcpos);
              dcpos = 0;
            }
            dcbuf[dcpos] = koi;
            dcpos += 1;
          }
        }
      }
      if (dcpos != 0) term_write_raw(term, dcbuf, dcpos);
    } else {
      term_write_raw(term, str, -1);
    }
  }
}


//==========================================================================
//
//  term_write_sanitize
//
//  sanisizes unicode
//
//==========================================================================
static void term_write_sanitize (Term *term, const char *str, int slen) {
  if (str && str[0] && slen > 0 &&
      term != NULL && term->deadstate == DS_ALIVE && term->child.cmdfd >= 0)
  {
    char utx[4];
    uint32_t cp = 0;
    while (slen > 0 && term->wrbuf.used < 1024*1024*32) { // arbitrary limit
      // try to flush the buffer
      if (term->wrbuf.used > 65535) {
        #if 0
        fprintf(stderr, "...trying to send %u bytes...\n", term->wrbuf.used);
        #endif
        one_term_write(term);
      }
      cp = yterm_utf8d_consume(cp, *str); str += 1; slen -= 1;
      if (yterm_utf8_valid_cp(cp) && cp != 0 && cp != 127) {
        if (!yterm_utf8_printable_cp(cp)) cp = YTERM_UTF8_REPLACEMENT_CP;
        else if (cp < 32 && cp != '\t' && cp != '\r' && cp != '\n') cp = YTERM_UTF8_REPLACEMENT_CP;
        if (koiLocale) {
          // utf -> koi
          char koi = yterm_uni2koi(cp);
          if (koi > 0 && koi < 32 && koi != '\t' && koi != '\r' && koi != '\n') koi = 0;
          if (koi && koi != 127) term_write_raw(term, &koi, 1);
        } else {
          const uint32_t ulen = yterm_utf8_encode(utx, cp);
          term_write_raw(term, utx, (int)ulen);
        }
      }
    }
  }
}


//==========================================================================
//
//  term_send_focus_event
//
//==========================================================================
static void term_send_focus_event (Term *term, yterm_bool focused) {
  if (term != NULL) {
    if ((term->mode & YTERM_MODE_FOCUSEVT) != 0) {
      if (focused) {
        term_write_raw(term, "\x1b[I", -1);
      } else {
        term_write_raw(term, "\x1b[O", -1);
      }
    }
    // reset modifiers on blur, don't bother properly tracking them
    if (!focused) {
      if (term->reportMods && term->lastModKeys != 0) term_write_raw(term, "\x1b[1Y", -1);
      term->lastModKeys = 0;
    }
  }
}


//==========================================================================
//
//  term_send_mouse
//
//==========================================================================
static void term_send_mouse (Term *term, int x, int y, int event, int button, int estate) {
  if (term != NULL && term->deadstate == DS_ALIVE && term->child.cmdfd >= 0 &&
      (term->mode & YTERM_MODE_MOUSE) != 0 &&
      x >= 0 && y >= 0 && x < TERM_CBUF(term)->width && y < TERM_CBUF(term)->height)
  {
    char buf[32];
    char *p;
    char lastCh = 'M';
    int ss;

    /* modes:
      1000 -- X11 xterm mouse reporting
      1005 -- utf-8 mouse encoding
      1006 -- sgr mouse encoding
      1015 -- urxvt mouse encoding
    */
    //sprintf(buf, "\x1b[M%c%c%c", 0, 32+x+1, 32+y+1);
    snprintf(buf, sizeof(buf), "\x1b[M");
    p = buf + strlen(buf);

    if (event == YTERM_MR_MOTION) {
      if ((term->mode & YTERM_MODE_MOUSEMOTION) != 0 &&
          (x != term->mreport.lastx || y != term->mreport.lasty))
      {
        button = term->mreport.lastbtn + 32;
        term->mreport.lastx = x;
        term->mreport.lasty = y;
      }
    } else if (event == YTERM_MR_UP) {
      if (term->mreport.mode != 1006) {
        button = 3; // 'release' flag
      } else {
        lastCh = 'm';
        button -= YTERM_MR_LEFT;
        if (button >= 3) button += 64 - 3;
      }
    } else {
      yterm_assert(event == YTERM_MR_DOWN);
      button -= YTERM_MR_LEFT;
      if (button >= 3) button += 64 - 3;
      term->mreport.lastbtn = button;
      term->mreport.lastx = x;
      term->mreport.lasty = y;
    }

    ss =
      (estate&Mod_Shift ? 4 : 0) |
      (estate&Mod_Alt ? 8 : 0) |
      (estate&Mod_Ctrl ? 16 : 0);

    switch (term->mreport.mode) {
      case 1006: /* sgr */
        p[-1] = '<';
        sprintf(p, "%d;", button+ss);
        p += strlen(p);
        break;
      case 1015: /* urxvt */
        p -= 1; // remove 'M'
        sprintf(p, "%d;", 32 + button + ss);
        p += strlen(p);
        break;
      default:
        *p = 32 + button + ss;
        p += 1;
        break;
    }

    // coords
    switch (term->mreport.mode) {
      case 1005: /* utf-8 */
        p += yterm_utf8_encode(p, (uint32_t)x + 1);
        p += yterm_utf8_encode(p, (uint32_t)y + 1);
        break;
      case 1006: /* sgr */
        sprintf(p, "%d;%d%c", x + 1, y + 1, lastCh);
        p += strlen(p);
        break;
      case 1015: /* urxvt */
        sprintf(p, "%d;%dM", x + 1, y + 1);
        p += strlen(p);
        break;
      default:
        sprintf(p, "%c%c", 32 + x + 1, 32 + y + 1);
        p += strlen(p);
        break;
    }
    *p = 0;

    term_write_raw(term, buf, -1);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  YTERM_ESC_NONE,
  YTERM_ESC_ESC, // seen ESC

  YTERM_ESC_CSI,
  YTERM_ESC_CSI_IGNORE,

  YTERM_ESC_ALT_CHARSET_G0,
  YTERM_ESC_ALT_CHARSET_G1,
  YTERM_ESC_HASH,
  YTERM_ESC_PERCENT,

  YTERM_ESC_OSC,
  YTERM_ESC_DCS,

  YTERM_ESC_S_IGN, // string, ignore
};


//==========================================================================
//
//  reset_esc
//
//  should touch only state!
//
//==========================================================================
YTERM_STATIC_INLINE void reset_esc (Term *term) {
  term->escseq.state = YTERM_ESC_NONE;
}


//==========================================================================
//
//  start_csi
//
//  prepare to parse CSI sequense
//
//==========================================================================
YTERM_STATIC_INLINE void start_csi (Term *term) {
  term->escseq.state = YTERM_ESC_CSI;
  term->escseq.argc = 0;
  term->escseq.priv = 0;
  term->escseq.cmd = 0;
}


//==========================================================================
//
//  start_osc
//
//==========================================================================
YTERM_STATIC_INLINE void start_osc (Term *term) {
  term->escseq.state = YTERM_ESC_OSC;
  term->escseq.sbufpos = 0;
  term->escseq.cmd = 0;
  term->escseq.priv = 0; // accum digits
  term->escseq.sbuf[0] = 0; // just in case
}


//==========================================================================
//
//  start_dcs
//
//==========================================================================
YTERM_STATIC_INLINE void start_dcs (Term *term) {
  term->escseq.state = YTERM_ESC_DCS;
  term->escseq.sbufpos = 0;
  term->escseq.cmd = 0;
  term->escseq.priv = 0; // accum digits
  term->escseq.sbuf[0] = 0; // just in case
}


//==========================================================================
//
//  finish_osc
//
//==========================================================================
static void finish_osc (Term *term) {
  if (term->escseq.state == YTERM_ESC_OSC) {
    #if 0
    fprintf(stderr, "OSC: cmd=%d; ntt=%s\n", term->escseq.cmd, term->escseq.sbuf);
    #endif
    if (term->escseq.cmd == 2) {
      char *ntt = term->escseq.sbuf;
      for (unsigned char *tmp = (unsigned char *)ntt; *tmp; tmp += 1) {
        if (*tmp < 32 || *tmp == 127) *tmp = ' ';
      }
      // remove trailing spaces
      size_t nlen = strlen(ntt);
      while (nlen > 0 && ((unsigned char *)ntt)[nlen - 1] == 32) --nlen;
      ntt[nlen] = 0;
      // remove leading spaces
      nlen = 0;
      while (ntt[nlen] && ((unsigned char *)ntt)[nlen] == 32) nlen += 1;
      if (nlen != 0) memmove(ntt, ntt + nlen, strlen(ntt + nlen) + 1);
      if (ntt[0] == 0) {
        // restore default title
        term->title.custom = 0;
        snprintf(term->title.last, sizeof(term->title.last), "%s", opt_title);
        term_check_title(term);
        if (winMapped && term->active) {
          x11_change_title(term->title.last);
        }
      } else {
        // set new custom title
        term_check_pgrp(term, 0);
        term->title.custom = 1;
        strcpy(term->title.last, ntt);
        if (winMapped && term->active) {
          x11_change_title(ntt);
        }
      }
      force_tab_redraw();
    }
  }
}


#include "term_lib_csi.inc.c"


//==========================================================================
//
//  term_process_altg0
//
//==========================================================================
static void term_process_altg0(Term *term, uint32_t cp) {
  if (opt_dump_esc_enabled) {
    fprintf(stderr, "ESC-DUMP: ESC-G0 %c (%u)\n", (cp > 32 && cp < 127 ? (char)cp : '.'), cp);
  }
  reset_esc(term);
  switch (cp) {
    case '0': /* Line drawing crap */
      term->mode |= YTERM_MODE_GFX0;
      break;
    case 'B': /* Back to regular text */
    case 'U': /* character ROM */
    case 'K': /* user mapping */
      term->mode &= ~YTERM_MODE_GFX0;
      break;
    default:
      fprintf(stderr, "esc unhandled charset: ESC ( %c\n",
              (cp > 0x20 && cp < 0x7f ? (char)cp : '.'));
      term->mode &= ~YTERM_MODE_GFX0;
      break;
  }
}


//==========================================================================
//
//  term_process_altg1
//
//==========================================================================
static void term_process_altg1 (Term *term, uint32_t cp) {
  if (opt_dump_esc_enabled) {
    fprintf(stderr, "ESC-DUMP: ESC-G1 %c (%u)\n", (cp > 32 && cp < 127 ? (char)cp : '.'), cp);
  }
  reset_esc(term);
  switch (cp) {
    case '0': /* Line drawing crap */
      term->mode |= YTERM_MODE_GFX1;
      break;
    case 'B': /* Back to regular text */
    case 'U': /* character ROM */
    case 'K': /* user mapping */
      term->mode &= ~YTERM_MODE_GFX1;
      break;
    default:
      fprintf(stderr, "esc unhandled charset: ESC ) %c\n",
              (cp > 0x20 && cp < 0x7f ? (char)cp : '.'));
      term->mode &= ~YTERM_MODE_GFX1;
      break;
  }
}


//==========================================================================
//
//  term_process_percent
//
//==========================================================================
static void term_process_percent (Term *term, uint32_t cp) {
  if (opt_dump_esc_enabled) {
    fprintf(stderr, "ESC-DUMP: ESC-%% %c (%u)\n", (cp > 32 && cp < 127 ? (char)cp : '.'), cp);
  }
  reset_esc(term);
  switch (cp) {
    case 'G': case '8':
      term->mode |= YTERM_MODE_FORCED_UTF;
      break;
    case '@':
      term->mode &= ~YTERM_MODE_FORCED_UTF;
      break;
    default:
      fprintf(stderr, "esc unhandled charset: ESC %% %c\n",
              (cp > 0x20 && cp < 0x7f ? (char)cp : '.'));
      term->mode &= ~YTERM_MODE_FORCED_UTF;
      break;
  }
}


//==========================================================================
//
//  term_process_hash
//
//==========================================================================
static void term_process_hash (Term *term, uint32_t cp) {
  if (opt_dump_esc_enabled) {
    fprintf(stderr, "ESC-DUMP: ESC-# %c (%u)\n", (cp > 32 && cp < 127 ? (char)cp : '.'), cp);
  }
  reset_esc(term);
  switch (cp) {
    case '8': /* DECALN -- DEC screen alignment test -- fill screen with E's */
      for (int y = 0; y < TERM_CBUF(term)->height; y += 1) {
        cbuf_write_wchar_count(TERM_CBUF(term), 0, y, 'E',
                               TERM_CBUF(term)->width, TERM_CATTR(term));
      }
      break;
    default:
      fprintf(stderr, "esc unhandled test: ESC # %c\n",
              (cp > 0x20 && cp < 0x7f ? (char)cp : '.'));
      term->mode &= ~YTERM_MODE_FORCED_UTF;
      break;
  }
}


//==========================================================================
//
//  term_process_osc
//
//==========================================================================
static void term_process_osc (Term *term, uint32_t cp) {
  yterm_assert(term->escseq.state == YTERM_ESC_OSC);
  // terminate on some control chars too (workaround for some buggy software)
  switch (cp) {
    case 0x07:
      finish_osc(term);
      reset_esc(term);
      return;
    case 0x08: // BS
    case 0x09: // BS
    case 0x0A: // LF
    case 0x0B: // VT
    case 0x0C: // FF
    case 0x0D: // CR
      reset_esc(term);
      term_putc(term, cp);
      return;
    case 0x0E:
      reset_esc(term);
      term->charset = YTERM_MODE_GFX1;
      return;
    case 0x0F:
      reset_esc(term);
      term->charset = YTERM_MODE_GFX0;
      return;
    case 0x18: // 0x18 aborts the sequence
      reset_esc(term);
      return;
    case 0x1A: // 0x1a aborts the sequence
      // 0x1a should print a reversed question mark (DEC does this), but meh
      reset_esc(term);
      term_putc(term, '?');
      return;
    case 0x1B: // ESC: finish current sequence, and restart a new one
      finish_osc(term);
      reset_esc(term);
      term->escseq.state = YTERM_ESC_ESC;
      return;
    case 0x7F: // ignore
      return;
  }

  if (cp < 0x20) return; // ignore other control chars

  // normal char
  if (cp >= '0' && cp <= '9' && term->escseq.priv >= 0) {
    // command digit
    term->escseq.cmd = min2(255, term->escseq.cmd * 10 + ((int)cp - '0'));
    term->escseq.priv = min2(255, term->escseq.priv + 1);
    #if 0
    fprintf(stderr, "OSC-DIGIT: cmd=%d; priv=%d (%c)\n",
            term->escseq.cmd, term->escseq.priv, (char)cp);
    #endif
  } else if (cp == ';' && term->escseq.priv >= 0) {
    // we're done with the command
    term->escseq.priv = -1;
    switch (term->escseq.cmd) {
      case 0: case 2: term->escseq.cmd = 2; break;
      default: term->escseq.cmd = 0; break;
    }
  } else {
    // collect command chars for title
    if (term->escseq.priv >= 0) {
      // bad command -- no ";" code terminator
      term->escseq.priv = -1;
      term->escseq.cmd = 0;
    }
    if (term->escseq.cmd == 2) {
      // always use utf-8 for titles
      char utb[4];
      const int xlen = (int)yterm_utf8_encode(utb, cp);
      if (term->escseq.sbufpos + xlen < (int)sizeof(term->escseq.sbuf) - 1) {
        memcpy(term->escseq.sbuf + term->escseq.sbufpos, utb, (unsigned)xlen);
        term->escseq.sbufpos += xlen;
        term->escseq.sbuf[term->escseq.sbufpos] = 0;
        #if 0
        fprintf(stderr, "OSC-CHAR: cmd=%d; priv=%d (%c)\n",
                term->escseq.cmd, term->escseq.priv, (char)cp);
        #endif
      } else {
        term->escseq.sbufpos = (int)sizeof(term->escseq.sbuf);
      }
    }
  }
}


//==========================================================================
//
//  term_process_s_ign
//
//==========================================================================
static void term_process_s_ign (Term *term, uint32_t cp) {
  yterm_assert(term->escseq.state == YTERM_ESC_S_IGN);
  // terminate on some control chars too (workaround for some buggy software)
  switch (cp) {
    case 0x07:
      reset_esc(term);
      return;
    case 0x08: // BS
    case 0x09: // BS
    case 0x0A: // LF
    case 0x0B: // VT
    case 0x0C: // FF
    case 0x0D: // CR
      reset_esc(term);
      term_putc(term, cp);
      return;
    case 0x0E:
      reset_esc(term);
      term->charset = YTERM_MODE_GFX1;
      return;
    case 0x0F:
      reset_esc(term);
      term->charset = YTERM_MODE_GFX0;
      return;
    case 0x18: // 0x18 aborts the sequence
      reset_esc(term);
      return;
    case 0x1A: // 0x1a aborts the sequence
      // 0x1a should print a reversed question mark (DEC does this), but meh
      reset_esc(term);
      term_putc(term, '?');
      return;
    case 0x1B: // ESC: finish current sequence, and restart a new one
      reset_esc(term);
      term->escseq.state = YTERM_ESC_ESC;
      return;
  }
  // eat all other chars
}


//==========================================================================
//
//  term_process_query_string
//
//==========================================================================
static void term_process_query_string (Term *term) {
  if (term->escseq.sbufpos == 0 ||
      term->escseq.sbufpos >= (int)sizeof(term->escseq.sbuf))
  {
    goto error;
  }

  char buf[256];

  const char *str = term->escseq.sbuf;
  yterm_assert(str[term->escseq.sbufpos] == 0); // just in case

  #if 0
  fprintf(stderr, "Q:<%s>\n", str);
  #endif

  //TODO: proper parser (but i don't know what the fuck should be here)
  if (str[0] != '$') goto error;
  str += 1;
  if (str[0] == 'q' && str[1] == 'm' && str[2] == 0) {
    buf[0] = 0;
    snprintf(buf, sizeof(buf), "%s", "\x1bP1$r");
    char *p = buf + strlen(buf);

    if (term->wkscr->currattr.fg & CELL_ATTR_MASK) {
      const uint32_t fg = (term->wkscr->currattr.fg & ~CELL_ATTR_MASK);
      if (fg < 8) sprintf(p, "%u;", 30 + fg);
      else if (fg < 16) sprintf(p, "%u;", 90 + (fg - 8));
      else if (fg < 256) sprintf(p, "38;5;%u;", fg);
    } else {
      const uint32_t fg = term->wkscr->currattr.fg;
      sprintf(p, "38;2;%u;%u;%u", (fg >> 16) & 0xff, (fg >> 8) & 0xff, fg & 0xff);
    }
    p += strlen(p);

    if (term->wkscr->currattr.bg & CELL_ATTR_MASK) {
      const uint32_t bg = (term->wkscr->currattr.bg & ~CELL_ATTR_MASK);
      if (bg < 8) sprintf(p, "%u;", 30 + bg);
      else if (bg < 16) sprintf(p, "%u;", 90 + (bg - 8));
      else if (bg < 256) sprintf(p, "38;5;%u;", bg);
    } else {
      const uint32_t bg = term->wkscr->currattr.bg;
      sprintf(p, "38;2;%u;%u;%u", (bg >> 16) & 0xff, (bg >> 8) & 0xff, bg & 0xff);
    }
    p += strlen(p);

    if (p[-1] == ';') p -= 1;
    #if 0
    fprintf(stderr, "R:<%s>\n", buf + 1);
    #endif
    strcpy(p, "m\x1b[\\");

    return;
  }

error:
  // "not recognized" (i guess)
  if (!opt_workaround_vim_idiocity) {
    term_write_raw(term, "\x1bP0$r\x1b[\\", -1);
  }
}


//==========================================================================
//
//  term_process_dcs
//
//==========================================================================
static void term_process_dcs (Term *term, uint32_t cp) {
  yterm_assert(term->escseq.state == YTERM_ESC_DCS);
  if (cp == 0x18 || cp == 0x1a || cp == 0x1b) {
    reset_esc(term);
    if (cp == 0x1b) {
      term_process_query_string(term);
      term->escseq.state = YTERM_ESC_ESC;
    }
  } else {
    char utb[4];
    const int xlen = (int)yterm_utf8_encode(utb, cp);
    if (term->escseq.sbufpos + xlen < (int)sizeof(term->escseq.sbuf) - 1) {
      memcpy(term->escseq.sbuf + term->escseq.sbufpos, utb, (unsigned)xlen);
      term->escseq.sbufpos += xlen;
      term->escseq.sbuf[term->escseq.sbufpos] = 0;
      #if 0
      fprintf(stderr, "DCS-CHAR: cmd=%d; priv=%d (%c)\n",
              term->escseq.cmd, term->escseq.priv, (char)cp);
      #endif
    } else {
      term->escseq.sbufpos = (int)sizeof(term->escseq.sbuf);
    }
  }
}


//==========================================================================
//
//  term_process_csi
//
//==========================================================================
static void term_process_csi (Term *term, uint32_t cp) {
  // CSI processing -- ESC [
  #if 0
  fprintf(stderr, "CSI: st=%d; cp='%c'; argc=%d; priv=%d; cmd=%d\n",
          term->escseq.state, (cp >= 0x20 && cp < 0x7f ? (char)cp : '*'),
          term->escseq.argc, term->escseq.priv, term->escseq.cmd);
  #endif
  if (csi_next_char(term, cp) != 0) {
    #if 0
    fprintf(stderr, "CSI-HANDLE: st=%d; cp='%c'; argc=%d; priv=%d; cmd=%d\n",
            term->escseq.state, (cp >= 0x20 && cp < 0x7f ? (char)cp : '*'),
            term->escseq.argc, term->escseq.priv, term->escseq.cmd);
    #endif
    csi_handle(term);
  }
}


//==========================================================================
//
//  term_process_control_char
//
//  returns non-zero if char was eaten
//
//==========================================================================
static yterm_bool term_process_control_char (Term *term, uint32_t cp) {
  yterm_bool res;
  // OSC will do it itself, for others it is the same
  // actually, OSC *may* want the same processing, but meh
  if (term->escseq.state != YTERM_ESC_OSC && term->escseq.state != YTERM_ESC_DCS) {
    res = 1;
    switch (cp) {
      case 7:
        // k8: hack: do not bell if we are in ESC sequence
        if (term->escseq.state == YTERM_ESC_NONE) term_putc(term, 0x07);
        break;
      case 14:
        term->charset = YTERM_MODE_GFX1;
        break;
      case 15:
        term->charset = YTERM_MODE_GFX0;
        break;
      case 0x1a: // 0x18 and 0x1a aborts the sequence
        // 0x1a should print a reversed question mark (DEC does this), but meh
        term_putc(term, '?');
        /* fallthrough */
      case 0x18: // 0x18 and 0x1a aborts the sequence
        reset_esc(term);
        break;
      case 0x1b: // ESC: finish current sequence, and restart a new one
        finish_osc(term);
        reset_esc(term);
        term->escseq.state = YTERM_ESC_ESC;
        break;
      case 127:
        break; // ignore it
      default:
        if (cp < 0x20) {
          term_putc(term, cp);
        } else {
          res = 0;
        }
        break;
    }
  } else {
    res = 0;
  }
  return res;
}


//==========================================================================
//
//  term_process_esc
//
//  process first char after ESC (sequence start)
//
//==========================================================================
static void term_process_esc (Term *term, uint32_t cp) {
  if (opt_dump_esc_enabled) {
    fprintf(stderr, "ESC-DUMP: ESC %c (%u)\n", (cp > 32 && cp < 127 ? (char)cp : '.'), cp);
  }
  reset_esc(term);
  switch (cp) {
    case '[': start_csi(term); break;
    case ']': start_osc(term); break;
    case '(': term->escseq.state = YTERM_ESC_ALT_CHARSET_G0; break;
    case ')': term->escseq.state = YTERM_ESC_ALT_CHARSET_G1; break;
    case '#': term->escseq.state = YTERM_ESC_HASH; break;
    case '%': term->escseq.state = YTERM_ESC_PERCENT; break;
    case '\\': // noop sequence; used to finish OSCs
      break;
    case 'D': /* IND -- Linefeed */
      term_lcf_reset(term);
      // some sources says that it works only when cursor is in scroll area
      if (TERM_CPOS(term)->y >= TERM_SCTOP(term) &&
          TERM_CPOS(term)->y <= TERM_SCBOT(term))
      {
        if (TERM_CPOS(term)->y == TERM_SCBOT(term)) {
          term_scroll_up(term, 1);
        } else {
          term_curmove_rel(term, 0, 1);
        }
      }
      break;
    case 'E': /* NEL -- Next line */
      term_lcf_reset(term);
      term_putc(term, '\n');
      term_putc(term, '\r'); // always go to the first column
      break;
    case 'F': /* xterm extension: Cursor to lower left corner of screen */
      term_curmove_abs(term, 0, TERM_CBUF(term)->height - 1);
      break;
    case 'H': /* HTS -- set custom tab position */
      fprintf(stderr, "WARNING: ESC H (custom tab position) is not supported.\n");
      break;
    case 'M': /* RI -- Reverse linefeed */
      term_lcf_reset(term);
      // some sources says that it works only when cursor is in scroll area
      if (TERM_CPOS(term)->y >= TERM_SCTOP(term) &&
          TERM_CPOS(term)->y <= TERM_SCBOT(term))
      {
        if (TERM_CPOS(term)->y == TERM_SCTOP(term)) {
          term_scroll_down(term, 1);
        } else {
          term_curmove_rel(term, 0, -1);
        }
      }
      break;
    case 'P': /* DCS -- Device control string */
      reset_esc(term);
      start_dcs(term);
      break;
    case '^': /* PM -- Privacy message */
      reset_esc(term);
      // use DCS mode here, because both this and DCS are completely ignored
      term->escseq.state = YTERM_ESC_S_IGN;
      break;
    case 'c': /* RIS -- Reset to inital state */
      term_reset_mode(term);
      break;
    #if 0
    case 'l': /* xterm extension: lock memory above the cursor */
    case 'm': /* xterm extension: unlock memory */
    #endif
    case '=': /* DECPAM -- Application keypad */
      term->mode |= YTERM_MODE_APPKEYPAD;
      break;
    case '>': /* DECPNM -- Normal keypad */
      term->mode &= ~YTERM_MODE_APPKEYPAD;
      break;
    case '7': /* DECSC -- Save Cursor */
      /* Save current state (cursor coordinates, attributes, character sets pointed at by G0, G1) */
      term_save_cursor(term, SaveDEC);
      break;
    case '8': /* DECRC -- Restore Cursor */
      term_restore_cursor(term, SaveDEC);
      break;
    case 'Z': /* DEC private identification */
      term_write_raw(term, "\x1b[?1;2c", -1);
      break;
    default:
      fprintf(stderr, "erresc: unknown sequence ESC 0x%02X ('%c')\n",
              cp, (cp > 0x20 && cp < 0x7f ? (char)cp : '.'));
      break;
  }
  #if 0
  fprintf(stderr, "ESC-START: st=%d; cp=0%04XH\n", term->escseq.state, cp);
  #endif
}


//==========================================================================
//
//  term_process_char
//
//  process escape sequences and such
//
//==========================================================================
static void term_process_char (Term *term, uint32_t cp) {
  if (term == NULL) return;

  // debug dump
  if (opt_dump_fd >= 0 && opt_dump_fd_enabled) {
    char utshit[4];
    write(opt_dump_fd, utshit, yterm_utf8_encode(utshit, cp));
  }

  // process control chars
  if (!term_process_control_char(term, cp)) {
    // now parse cp according to the state
    switch (term->escseq.state) {
      case YTERM_ESC_NONE: term_putc(term, cp); break;
      case YTERM_ESC_ESC: term_process_esc(term, cp); break;
      case YTERM_ESC_ALT_CHARSET_G0: term_process_altg0(term, cp); break;
      case YTERM_ESC_ALT_CHARSET_G1: term_process_altg1(term, cp); break;
      case YTERM_ESC_HASH: term_process_hash(term, cp); break;
      case YTERM_ESC_PERCENT: term_process_percent(term, cp); break;
      case YTERM_ESC_CSI: term_process_csi(term, cp); break;
      case YTERM_ESC_CSI_IGNORE: term_process_csi(term, cp); break;
      case YTERM_ESC_OSC: term_process_osc(term, cp); break;
      case YTERM_ESC_DCS: term_process_dcs(term, cp); break;
      case YTERM_ESC_S_IGN: term_process_s_ign(term, cp); break;
      default: yterm_assert("ketmar forgot to process some ESC sequence parser state");
    }
  }

  #ifdef USELESS_DEBUG_FEATURES
  if (opt_debug_slowdown) {
    if (term->scroll_count != 0 || TERM_CBUF(term)->dirtyCount != 0) {
      term->scroll_count = 0;
      cbuf_mark_all_dirty(TERM_CBUF(term));
      CALL_RENDER(
        renderDirty(rcbuf, rcpos);
      );
      // debug slowdown
      XSync(x11_dpy, False);
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 50 * 1000000;
      nanosleep(&ts, NULL);
      XSync(x11_dpy, False);
    }
  }
  #endif
}


//==========================================================================
//
//  term_simple_print_ensure_nl
//
//==========================================================================
static void term_simple_print_ensure_nl (Term *term) {
  if (term == NULL) return;
  TERM_CATTR(term)->flags = 0;
  TERM_CATTR(term)->fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
  TERM_CATTR(term)->bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
  if (TERM_CPOS(term)->x != 0) {
    term_process_char(term, '\r');
    term_process_char(term, '\n');
  }
}


//==========================================================================
//
//  term_simple_print
//
//  used to print simple messages to dead terminals
//
//==========================================================================
static void term_simple_print (Term *term, const char *msg) {
  if (term == NULL) return;
  while (*msg) term_process_char(term, *(const unsigned char *)msg++);
}
