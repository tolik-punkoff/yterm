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
  selection mode.
  included directly from "x11_keypress.inc.c"
*/


//==========================================================================
//
//  term_has_selblock
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool term_has_selblock (Term *term) {
  return
    term != NULL && TERM_IN_SELECTION(term) &&
    term->history.y0 <= term->history.y1;
}


//==========================================================================
//
//  cy_to_gy
//
//  from screen coord to "global" history coord
//
//==========================================================================
YTERM_STATIC_INLINE int cy_to_gy (Term *term, int cy) {
  return (term->history.file.lcount - term->history.sback) + cy;
}


//==========================================================================
//
//  gy_to_cy
//
//  from "global" history coord to screen coord
//
//==========================================================================
YTERM_STATIC_INLINE int gy_to_cy (Term *term, int gy) {
  return gy - (term->history.file.lcount - term->history.sback);
}


//==========================================================================
//
//  term_selection_end
//
//==========================================================================
static void term_selection_end (Term *term) {
  if (term == NULL || !TERM_IN_SELECTION(term)) return;
  if (term->history.inprogress == 0) {
    // do nothing, we're already finished (or not started)
    //term->history.y0 = 0; term->history.y1 = -1;
  } else {
    const int stx = term->history.x0;
    const int sty = term->history.y0;
    int cx = term->history.cx;
    int cy = cy_to_gy(term, term->history.cy);
    if (term->history.blocktype == SBLOCK_STREAM) {
      if (cy == sty) {
        // same line
        term->history.y0 = sty;
        term->history.y1 = sty;
        term->history.x0 = min2(stx, cx);
        term->history.x1 = max2(stx, cx);
      } else if (cy < sty) {
        // above
        term->history.x0 = cx;
        term->history.y0 = cy;
        term->history.x1 = stx;
        term->history.y1 = sty;
      } else {
        // below
        term->history.x0 = stx;
        term->history.y0 = sty;
        term->history.x1 = cx;
        term->history.y1 = cy;
      }
    } else {
      term->history.x0 = min2(stx, cx);
      term->history.x1 = max2(stx, cx);
      term->history.y0 = min2(sty, cy);
      term->history.y1 = max2(sty, cy);
    }
    term->history.inprogress = 0;
  }
}


//==========================================================================
//
//  term_mark_selected_block
//
//==========================================================================
static void term_mark_selected_block (Term *term) {
  // temporarily finish the selection
  const yterm_bool old_inprogress = term->history.inprogress;
  const yterm_bool old_x0 = term->history.x0;
  const yterm_bool old_y0 = term->history.y0;
  const yterm_bool old_x1 = term->history.x1;
  const yterm_bool old_y1 = term->history.y1;
  term_selection_end(term);

  if (term_has_selblock(term) && hvbuf.width != 0) {
    for (int y = term->history.y0; y <= term->history.y1; y += 1) {
      const int wy = gy_to_cy(term, y);
      if (wy >= 0 && wy < hvbuf.height) {
        int x0 = 0, x1 = hvbuf.width - 1;
        CharCell *cc = &hvbuf.buf[wy * hvbuf.width];
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
        for (int x = x0; x <= x1; x += 1) cc[x].flags |= CELL_SELECTION;
      }
    }
  }

  // restore selection state
  term->history.inprogress = old_inprogress;
  term->history.x0 = old_x0;
  term->history.y0 = old_y0;
  term->history.x1 = old_x1;
  term->history.y1 = old_y1;
}


static uint32_t *sel_flags_buffer = NULL;
static size_t sel_flags_buffer_size = 0;


//==========================================================================
//
//  term_draw_selection
//
//==========================================================================
static void term_draw_selection (Term *term) {
  if (term == NULL || !TERM_IN_SELECTION(term)) return;

  // reset?
  if (hvbuf.width != TERM_CBUF(term)->width ||
      hvbuf.height != TERM_CBUF(term)->height)
  {
    cbuf_free(&hvbuf);
    cbuf_new(&hvbuf, TERM_CBUF(term)->width, TERM_CBUF(term)->height);
    memcpy(hvbuf.buf, TERM_CBUF(term)->buf,
           (unsigned)(TERM_CBUF(term)->width * TERM_CBUF(term)->height) * sizeof(CharCell));
    hvbuf.dirtyCount = TERM_CBUF(term)->dirtyCount;
    cbuf_mark_all_dirty(&hvbuf);
    term->history.sback = 0;
    term_mark_selected_block(term);
  } else {
    // remember selected flags
    const int hsz = hvbuf.width * hvbuf.height;
    const size_t sbs = (unsigned)hsz * sizeof(sel_flags_buffer[0]);
    if (sbs > sel_flags_buffer_size) {
      sel_flags_buffer = realloc(sel_flags_buffer, sbs);
      yterm_assert(sel_flags_buffer != NULL); // FIXME: handle this!
      sel_flags_buffer_size = sbs;
    }
    CharCell *cc = hvbuf.buf;
    for (int f = 0; f < hsz; f += 1) {
      sel_flags_buffer[f] = cc[f].flags;
    }

    // draw
    int y0 = 0;
    int hline = term->history.file.lcount - term->history.sback;
    #if 0
    fprintf(stderr, "DSEL: hline=%d; lcount: %d\n",
            hline, term->history.file.lcount);
    #endif
    while (y0 < hvbuf.height && hline < term->history.file.lcount) {
      history_put_line(y0, hline, &term->history.file);
      y0 += 1; hline += 1;
    }
    if (y0 < hvbuf.height) {
      CharCell *src = TERM_CBUF(term)->buf;
      #if 0
      fprintf(stderr, "DSEL: origlest: %d\n", hvbuf.height - y0);
      #endif
      int ty = 0;
      while (y0 < hvbuf.height && ty < TERM_CBUF(term)->height) {
        for (int x = 0; x < hvbuf.width; x += 1) {
          if (x < TERM_CBUF(term)->width) {
            cbuf_merge_cell(&hvbuf, x, y0, src);
            src += 1;
          }
        }
        ty += 1; y0 += 1;
      }
    }
    term_mark_selected_block(term);

    // mark cells with changed selected flags dirty
    for (int f = 0; f < hsz; f += 1) {
      if (((sel_flags_buffer[f] ^ cc[f].flags) & CELL_SELECTION) != 0) {
        if ((cc[f].flags & CELL_DIRTY) == 0) {
          cc[f].flags |= CELL_DIRTY;
          hvbuf.dirtyCount += 1;
        }
      }
    }
  }
}


//==========================================================================
//
//  term_selection_reset
//
//==========================================================================
static void term_selection_reset (Term *term) {
  if (term_has_selblock(term)) {
    term->history.x0 = 0; term->history.x1 = -1;
    term->history.y0 = 0; term->history.y1 = -1;
    term->history.inprogress = 0;
    term_draw_selection(term);
  }
}


//==========================================================================
//
//  term_selection_start
//
//==========================================================================
static void term_selection_start (Term *term) {
  if (term != NULL && TERM_IN_SELECTION(term)) {
    term->history.x0 = term->history.cx;
    term->history.y0 = cy_to_gy(term, term->history.cy);
    term->history.x1 = term->history.x0;
    term->history.y1 = term->history.y1;
    term->history.inprogress = 1;
    term_draw_selection(term);
  }
}


//==========================================================================
//
//  yterm_isword
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool yterm_isword (const uint32_t cp) {
  return (cp == '_' || cp == '$' || yterm_isalnum(cp));
}


//==========================================================================
//
//  xsel_word_left
//
//==========================================================================
static void xsel_word_left (Term *term) {
  int cx = term->history.cx;
  int cy = term->history.cy;

  if (cx == 0) {
    // left border
    if (term->history.blocktype != SBLOCK_STREAM) return;
    cbuf_mark_dirty(&hvbuf, cx, cy);
    if (cy != 0) {
      term->history.cx = hvbuf.width - 1;
      term->history.cy -= 1;
    } else if (term->history.sback < term->history.file.lcount) {
      term->history.cx = hvbuf.width - 1;
      term->history.sback += 1;
    }
  } else {
    cbuf_mark_dirty(&hvbuf, cx, cy);
    const int adr = cy * hvbuf.width;
    const yterm_bool okflag = yterm_isword(hvbuf.buf[adr + cx - 1].ch);
    while (cx != 0 && yterm_isword(hvbuf.buf[adr + cx - 1].ch) == okflag) cx -= 1;
    //if (!term->history.inprogress && cx != 0) cx -= 1;
  }

  term->history.cx = cx; term->history.cy = cy;
  if (term->history.inprogress) term_draw_selection(term);
  cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
}


//==========================================================================
//
//  xsel_word_right
//
//==========================================================================
static void xsel_word_right (Term *term) {
  int cx = term->history.cx;
  int cy = term->history.cy;

  if (cx == hvbuf.width - 1) {
    // right border
    if (term->history.blocktype != SBLOCK_STREAM) return;
    cbuf_mark_dirty(&hvbuf, cx, cy);
    if (cy != hvbuf.height - 1) {
      term->history.cx = 0;
      term->history.cy += 1;
    } else if (term->history.sback != 0) {
      term->history.cx = 0;
      term->history.sback -= 1;
    }
  } else {
    cbuf_mark_dirty(&hvbuf, cx, cy);
    const int adr = cy * hvbuf.width;
    const yterm_bool okflag = yterm_isword(hvbuf.buf[adr + cx + 1].ch);
    while (cx != hvbuf.width - 1 && yterm_isword(hvbuf.buf[adr + cx + 1].ch) == okflag) cx += 1;
    if (!term->history.inprogress && cx != hvbuf.width - 1) cx += 1;
  }

  term->history.cx = cx; term->history.cy = cy;
  if (term->history.inprogress) term_draw_selection(term);
  cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
}


//==========================================================================
//
//  xsel_is_blank
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool xsel_is_blank (uint16_t ch) {
  return (ch <= 32 || ch == 127 || (ch >= 128 && !yterm_isalnum(ch)));
}


//==========================================================================
//
//  xsel_altword_move
//
//==========================================================================
static void xsel_altword_move (Term *term, int dir) {
  yterm_assert(dir == -1 || dir == 1);
  const int cx = term->history.cx;
  const int cy = term->history.cy;
  const int adr = cy * hvbuf.width;
  yterm_bool stblank = xsel_is_blank(hvbuf.buf[adr + cx].ch);

  int nx = cx + dir;
  // if we are at the border, toggle flag meaning
  if (nx >= 0 && nx < hvbuf.width && xsel_is_blank(hvbuf.buf[adr + nx].ch) != stblank) {
    stblank = !stblank;
    nx += dir;
  }
  while (nx >= 0 && nx < hvbuf.width && xsel_is_blank(hvbuf.buf[adr + nx].ch) == stblank) {
    nx += dir;
  }
  nx -= dir;

  if (cx != nx) {
    cbuf_mark_dirty(&hvbuf, cx, cy);
    term->history.cx = nx;
    if (term->history.inprogress) term_draw_selection(term);
    cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
  }
}


//==========================================================================
//
//  x11_selection_curmove
//
//==========================================================================
static void x11_selection_curmove (Term *term, int dx, int dy) {
  if (dx != 0 || dy != 0) {
    cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);

    int nx = clamp(term->history.cx + dx, 0, TERM_CBUF(term)->width - 1);
    int ny = term->history.cy + dy;

    if (ny < 0) {
      int sback = min2(term->history.file.lcount, term->history.sback - ny);
      #if 0
      fprintf(stderr, "newsback: %d; sback: %d; lcount: %d\n",
              sback, term->history.sback, term->history.file.lcount);
      #endif
      term->history.cx = nx; term->history.cy = 0;
      if (sback != term->history.sback || term->history.inprogress) {
        term->history.sback = sback;
        term_draw_selection(term);
      }
      cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
      return;
    }

    if (ny >= hvbuf.height) {
      int sback = max2(0, term->history.sback - (ny - (hvbuf.height - 1)));
      term->history.cx = nx; term->history.cy = hvbuf.height - 1;
      if (sback != term->history.sback || term->history.inprogress) {
        term->history.sback = sback;
        term_draw_selection(term);
      }
      cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
      return;
    }

    if (nx != term->history.cx || ny != term->history.cy) {
      term->history.cx = nx;
      term->history.cy = ny;
      if (term->history.inprogress) term_draw_selection(term);
      cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
    }
  }
}


//==========================================================================
//
//  x11_parse_keyevent_selection
//
//==========================================================================
static void x11_parse_keyevent_selection (Term *term, XKeyEvent *e,
                                          KeySym ksym, uint32_t kmods)
{
  if (ksym == XK_Escape) {
    term->history.blocktype = SBLOCK_NONE;
    cbuf_mark_all_dirty(TERM_CBUF(term));
    return;
  }

  // check keybinds
  for (KeyBind *kb = keybinds; kb != NULL; kb = kb->next) {
    if (kb->mods == kmods && kb->ksym == ksym) {
      if (kb->killsignal > 255) {
        int kbx = -1;
        switch (kb->killsignal) {
          // pass the foillowing
          case KB_CMD_TAB_PREV:
          case KB_CMD_TAB_NEXT:
          case KB_CMD_TAB_FIRST:
          case KB_CMD_TAB_LAST:
          case KB_CMD_TAB_MOVE_LEFT:
          case KB_CMD_TAB_MOVE_RIGHT:
            do_keybind(term, kb);
            return;
          case KB_CMD_COPY_TO_PRIMARY: kbx = 0; break;
          case KB_CMD_COPY_TO_SECONDARY: kbx = 1; break;
          case KB_CMD_COPY_TO_CLIPBOARD: kbx = 2; break;
        }
        if (kbx != -1) {
          if (term->history.inprogress) term_selection_end(term);
          do_copy_to(term, kbx);
          if (term->history.locked == 0) {
            term->history.blocktype = SBLOCK_NONE;
            cbuf_mark_all_dirty(TERM_CBUF(term));
            term_selection_reset(term);
          } else {
            //term->history.locked = 0; // do not reset locked state
            term_selection_reset(term);
            term_draw_selection(term);
          }
          return;
        }
      } else if (kb->exec != NULL) {
        do_keybind(term, kb);
        return;
      }
    }
  }

  if ((e->state&Mod2Mask) == 0) ksym = translate_keypad(ksym);

  if (ksym == XK_space) {
    if (kmods & Mod_Ctrl) term_selection_start(term);
    else if (kmods & Mod_Alt) term_selection_reset(term);
    else if (term->history.inprogress == 0) term_selection_start(term);
    else term_selection_end(term);
    return;
  }

  if (ksym == XK_F3) {
    if (term->history.inprogress == 0) {
      term_selection_start(term);
      if (kmods & Mod_Shift) term->history.blocktype = SBLOCK_RECT;
      else if (kmods & Mod_Alt) term->history.blocktype = SBLOCK_LINE;
      else term->history.blocktype = SBLOCK_STREAM;
      // update to show selected line (it is already there)
      if (term->history.blocktype == SBLOCK_LINE) {
        term_draw_selection(term);
      }
    } else {
      term_selection_end(term);
    }
    return;
  }

  int dx = 0, dy = 0;
  switch (ksym) {
    case XK_Left:
      if ((kmods & Mod_Shift) && !term->history.inprogress) term_selection_start(term);
      if (kmods & Mod_Alt) {
        xsel_altword_move(term, -1);
      } else if (kmods & Mod_Ctrl) {
        xsel_word_left(term);
      } else if (term->history.cx == 0 && term->history.blocktype == SBLOCK_STREAM) {
        dx = hvbuf.width - 1;
        dy = -1;
      } else {
        dx = -1;
      }
      break;
    case XK_Right:
      if ((kmods & Mod_Shift) && !term->history.inprogress) term_selection_start(term);
      if (kmods & Mod_Alt) {
        xsel_altword_move(term, 1);
      } else if (kmods & Mod_Ctrl) {
        xsel_word_right(term);
      } else if (term->history.cx == hvbuf.width - 1 && term->history.blocktype == SBLOCK_STREAM) {
        dx = -term->history.cx;
        dy = 1;
      } else {
        dx = 1;
      }
      break;
    case XK_Up:
      if ((kmods & Mod_Shift) && !term->history.inprogress) term_selection_start(term);
      if (kmods & Mod_Alt) dy = -10; else dy = -1;
      if (kmods & Mod_Ctrl) {
        // shift up
        if (term->history.file.lcount != 0) {
          term->history.sback = min2(term->history.sback - dy, term->history.file.lcount - 1);
          term_draw_selection(term);
        }
        dy = 0;
      }
      break;
    case XK_Down:
      if ((kmods & Mod_Shift) && !term->history.inprogress) term_selection_start(term);
      if (kmods & Mod_Alt) dy = 10; else dy = 1;
      if (kmods & Mod_Ctrl) {
        // shift down
        if (term->history.file.lcount != 0) {
          term->history.sback = max2(term->history.sback - dy, 0);
          term_draw_selection(term);
        }
        dy = 0;
      }
      break;
    case XK_Home:
      if (kmods & Mod_Ctrl) {
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term->history.cx = 0; term->history.cy = 0;
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term_draw_selection(term);
      } else if (term->history.cx != 0) {
        const int adr = term->history.cy * hvbuf.width;
        int cx = 0;
        while (cx != hvbuf.width - 1 && !yterm_isword(hvbuf.buf[adr + cx].ch)) cx += 1;
        if (cx >= term->history.cx) {
          dx = -term->history.cx;
        } else {
          dx = cx - term->history.cx;
        }
      }
      break;
    case XK_End:
      if (kmods & Mod_Ctrl) {
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term->history.cx = hvbuf.width - 1; term->history.cy = hvbuf.height - 1;
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term_draw_selection(term);
      } else {
        if ((kmods & Mod_Shift) && !term->history.inprogress) term_selection_start(term);
        if (term->history.cx != hvbuf.width - 1) {
          const int adr = term->history.cy * hvbuf.width;
          int cx = hvbuf.width - 1;
          while (cx != 0 && !yterm_isword(hvbuf.buf[adr + cx].ch)) cx -= 1;
          if (cx <= term->history.cx) {
            dx = hvbuf.width - 1 - term->history.cx;
          } else {
            dx = cx - term->history.cx;
          }
        }
      }
      break;
    case XK_Page_Up:
      if (kmods & Mod_Alt) {
        // shift up
        if (term->history.file.lcount != 0) {
          term->history.sback = min2(term->history.sback + (TERM_CBUF(term)->height - 1),
                                     term->history.file.lcount - 1);
          term_draw_selection(term);
        }
      } else if (kmods & Mod_Ctrl) {
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term->history.sback = term->history.file.lcount;
        term->history.cx = 0; term->history.cy = 0;
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term_draw_selection(term);
      } else {
        if ((kmods & Mod_Shift) && !term->history.inprogress) term_selection_start(term);
        if (term->history.cy != 0) dy = -term->history.cy;
        else dy = -(hvbuf.height - 1);
      }
      break;
    case XK_Page_Down:
      if (kmods & Mod_Alt) {
        // shift down
        if (term->history.file.lcount != 0) {
          term->history.sback = max2(term->history.sback - (TERM_CBUF(term)->height - 1), 0);
          term_draw_selection(term);
        }
      } else if (kmods & Mod_Ctrl) {
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term->history.sback = 0;
        term->history.cx = hvbuf.width - 1; term->history.cy = hvbuf.height - 1;
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
        term_draw_selection(term);
      } else {
        if ((kmods & Mod_Shift) && !term->history.inprogress) term_selection_start(term);
        if (term->history.cy != hvbuf.height - 1) dy = hvbuf.height - term->history.cy - 1;
        else dy = hvbuf.height - 1;
      }
      break;
    case XK_r: case XK_R: // rect
      term->history.blocktype = SBLOCK_RECT;
      term_draw_selection(term);
      break;
    case XK_l: case XK_L: // line
      term->history.blocktype = SBLOCK_LINE;
      term_draw_selection(term);
      break;
    case XK_s: case XK_S: // stream
      term->history.blocktype = SBLOCK_STREAM;
      term_draw_selection(term);
      break;
    case XK_k: case XK_K: // lock
      term->history.locked = 1;
      break;
    case XK_u: case XK_U: // unlock
      term->history.locked = 0;
      break;
    // send mouse click
    case XK_Return:
    case XK_KP_Enter:
      if (term->history.inprogress == 0 && !term_has_selblock(term) &&
          term->history.sback == 0)
      {
        if ((term->mode & YTERM_MODE_MOUSE) != 0 &&
            term->deadstate == DS_ALIVE && term->child.cmdfd >= 0)
        {
          if ((kmods & Mod_Shift) == 0) {
            term->history.blocktype = SBLOCK_NONE;
            cbuf_mark_all_dirty(TERM_CBUF(term));
          }
          // send it
          term_send_mouse(term, term->history.cx, term->history.cy,
                          YTERM_MR_DOWN, 1/*left button*/,
                          0/*event->state*/);
          term_send_mouse(term, term->history.cx, term->history.cy,
                          YTERM_MR_UP, 1/*left button*/,
                          0/*event->state*/);
          // ...and again, if alt/ctrl is pressed, to emulate doubleclick
          if (kmods & (Mod_Ctrl | Mod_Alt)) {
            term_send_mouse(term, term->history.cx, term->history.cy,
                            YTERM_MR_DOWN, 1/*left button*/,
                            0/*event->state*/);
            term_send_mouse(term, term->history.cx, term->history.cy,
                            YTERM_MR_UP, 1/*left button*/,
                            0/*event->state*/);
          }
        }
      }
      break;
  }

  if (dx != 0 || dy != 0) {
    x11_selection_curmove(term, dx, dy);
  }
}
