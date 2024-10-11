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
  X11 tab bar.
  included directly into the main file.
*/


typedef struct {
  uint32_t text;
  uint32_t index;
  uint32_t ellipsis;
  uint32_t bg;
} TabColors;

enum {
  TABC_NORMAL,
  TABC_ACTIVE,
  TABC_DEAD,
  TABC_DEAD_ACTIVE,
};

static uint32_t tab_frame_fg = 0x607a50;
static uint32_t tab_frame_bg = 0x333333;

static TabColors tab_colors[4] = {
  // normal
  {.text=0x809a70, .index=0xa0ba90, .ellipsis=0x607a50, .bg=0x333333},
  // active
  {.text=0x666666, .index=0x868686, .ellipsis=0x464646, .bg=0x000000},
  // dead
  {.text=0xeeee00, .index=0xffff00, .ellipsis=0xbbbb00, .bg=0xcc0000},
  // dead, and active
  {.text=0xee0000, .index=0xff0000, .ellipsis=0xaa0000, .bg=0x330000},
};


//==========================================================================
//
//  repaint_tabs
//
//==========================================================================
static void repaint_tabs (int x0, int width) {
  if (!opt_enable_tabs || winTabsHeight < 2 || x11_tabsfont.fst == NULL) return;
  if (width < 1) return;
  if (x0 >= winWidth || (x0 < 0 && x0 + width <= 0)) return;
  if (x0 < 0) {
    width += x0; x0 = 0;
    if (width < 1) return;
  }
  if (winWidth - x0 < width) {
    width = winWidth - x0;
    if (width < 1) return;
  }

  int y0 = winHeight;
  const int rtabh = max2(0, winRealHeight - y0) - 1;
  const int tabh = max2(winTabsHeight - 1, rtabh);

  SET_FG_COLOR(tab_frame_fg);
  XDrawLine(x11_dpy, (Drawable)x11_win, x11_gc, x0, y0, x0 + width, y0);

  const char *ellipsis = "...";
  const int ellw = XTextWidth(x11_tabsfont.fst, ellipsis, 3);

  if (rtabh > 0) {
    XRectangle crect[1];
    XSetFont(x11_dpy, x11_gc, x11_tabsfont.fst->fid);
    y0 += 1; // skip top frame
    int ty = max2(0, (rtabh - (x11_tabsfont.ascent + x11_tabsfont.descent)) / 2);
    ty = y0 + x11_tabsfont.ascent;
    int lastPaintX = -0x7fffffff;
    for (Term *term = termlist; term != NULL; term = term->next) {
      if (term->tab.width != 0) {
        const int sx = term->tab.posx;
        const int sw = term->tab.width;
        if (sx + sw > x0 || sx < x0 + width) {
          lastPaintX = max2(lastPaintX, sx + sw);

          // color scheme
          int scheme;
          if (term == currterm) {
            if (term->deadstate != DS_ALIVE) scheme = TABC_DEAD_ACTIVE;
            else scheme = TABC_ACTIVE;
          } else if (term->deadstate != DS_ALIVE) {
            scheme = TABC_DEAD;
          } else {
            scheme = TABC_NORMAL;
          }

          // erase background
          SET_FG_COLOR(tab_colors[scheme].bg);
          XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc, sx, y0, sw - 1, tabh);

          // draw tab delimiter (last tab pixel)
          SET_FG_COLOR(tab_frame_fg);
          XDrawLine(x11_dpy, (Drawable)x11_win, x11_gc, sx + sw - 1, y0, sx + sw - 1, y0 + tabh);

          // set clip rect to the whole tab
          crect[0].x = sx + 1; crect[0].y = y0;
          crect[0].width = sw - 2; crect[0].height = tabh;
          XSetClipRectangles(x11_dpy, x11_gc, 0, 0, crect, 1, YXBanded);

          // draw number
          SET_FG_COLOR(tab_colors[scheme].index);
          XDrawString(x11_dpy, (Drawable)x11_win, x11_gc, sx + 2, ty,
                      term->tab.textnum, term->tab.numlen);

          // set clip rect to the rest of the tab
          int wrest = sw - (term->tab.numwidth + 4);
          if (wrest > 2) {
            // draw title text
            SET_FG_COLOR(tab_colors[scheme].text);

            if (wrest < ellw * 4 || term->tab.textwidth <= wrest) {
              // it fits, or cannot draw ellipsis
              crect[0].x = sx + 1 + term->tab.numwidth + 1; crect[0].y = y0;
              crect[0].width = wrest; crect[0].height = tabh;
              XSetClipRectangles(x11_dpy, x11_gc, 0, 0, crect, 1, YXBanded);

              XDrawString16(x11_dpy, (Drawable)x11_win, x11_gc, crect[0].x, ty,
                            term->tab.text, term->tab.textlen);
            } else {
              // doesn't fit, can draw ellipsis

              const int xell = wrest / 2 - ellw / 2;

              // draw left part
              crect[0].x = sx + 1 + term->tab.numwidth + 1; crect[0].y = y0;
              crect[0].width = xell; crect[0].height = tabh;
              XSetClipRectangles(x11_dpy, x11_gc, 0, 0, crect, 1, YXBanded);

              XDrawString16(x11_dpy, (Drawable)x11_win, x11_gc, crect[0].x, ty,
                            term->tab.text, term->tab.textlen);

              // draw right part
              crect[0].x = sx + sw - 2 - xell; crect[0].y = y0;
              crect[0].width = sx + sw - crect[0].x; crect[0].height = tabh;
              XSetClipRectangles(x11_dpy, x11_gc, 0, 0, crect, 1, YXBanded);

              XDrawString16(x11_dpy, (Drawable)x11_win, x11_gc,
                            sx + sw - 1 - term->tab.textwidth, ty,
                            term->tab.text, term->tab.textlen);

              // draw ellipsis
              SET_FG_COLOR(tab_colors[scheme].ellipsis);

              crect[0].x = sx + 1 + term->tab.numwidth + 1 + xell; crect[0].y = y0;
              crect[0].width = ellw; crect[0].height = tabh;
              XSetClipRectangles(x11_dpy, x11_gc, 0, 0, crect, 1, YXBanded);

              XDrawString(x11_dpy, (Drawable)x11_win, x11_gc, crect[0].x, ty,
                          ellipsis, 3);
            }
          }

          //XDrawString16(x11_dpy, (Drawable)x11_win, x11_gc, winx + 1, winy, tmpbuf, written);
          XSetClipMask(x11_dpy, x11_gc, None);
        }
      }
    }

    if (lastPaintX >= x0 && lastPaintX < x0 + width) {
      SET_FG_COLOR(tab_frame_bg);
      XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc,
                     lastPaintX, y0, x0 + width - lastPaintX, tabh);
    }

    XSetFont(x11_dpy, x11_gc, x11_font.fst->fid);
  }
}


//==========================================================================
//
//  make_current_tab_visible
//
//  returns non-zero if something was changed
//
//==========================================================================
static yterm_bool make_current_tab_visible (void) {
  if (!opt_enable_tabs || winTabsHeight < 2 || x11_tabsfont.fst == NULL) return 0;
  if (currterm == NULL || currterm->tab.width == 0) return 0; // just in case

  // check if it is visible
  int deltax = 0;
  if (currterm->tab.posx < 0) {
    // at the left, shift so it will become the first tab
    deltax = -currterm->tab.posx;
  } else if (currterm->tab.posx + currterm->tab.width > winWidth) {
    // at the right, shift so it will become the last tab
    const int newx = winWidth - currterm->tab.width;
    deltax = newx - currterm->tab.posx;
  }

  // shift tabs
  if (deltax != 0) {
    for (Term *term = termlist; term != NULL; term = term->next) {
      if (term->tab.width != 0) term->tab.posx += deltax;
    }
  }

  return (deltax != 0);
}


//==========================================================================
//
//  update_all_tabs
//
//  update tabs (if necessary), recalc new tab positions, redraw them
//
//==========================================================================
static void update_all_tabs () {
  need_update_tabs = 0;
  if (!opt_enable_tabs || winTabsHeight < 2 || x11_tabsfont.fst == NULL) return;

  // count alive tabs, and first visible tab
  int tc = 0, prevc = 0;
  Term *term;
  for (term = termlist; term != NULL; term = term->next) {
    if (term->deadstate != DS_DEAD) {
      if (term->tab.width != 0 && term->tab.posx < 0) prevc += 1;
      tc += 1;
    } else {
      term->tab.width = 0;
    }
  }

  if (tc == 0) return; // oops

  int tabw = winWidth / opt_tabs_visible;
  if (tabw < 8) {
    // no room for tabs, don't render them
    for (term = termlist; term != NULL; term = term->next) {
      term->tab.width = 0;
    }
    return;
  }

  // all tabs are visible
  if (tc <= opt_tabs_visible) prevc = 0;

  int tnum = 0;
  // leave first tab visible
  int posx = 0 - tabw * prevc;
  for (term = termlist; term != NULL; term = term->next) {
    if (term->deadstate != DS_DEAD) {
      // format number
      snprintf(term->tab.textnum, sizeof(term->tab.textnum), "[%d]", tnum);
      term->tab.numlen = strlen(term->tab.textnum);
      term->tab.numwidth = XTextWidth(x11_tabsfont.fst, term->tab.textnum, term->tab.numlen);
      // format title text
      uint32_t cp = 0;
      term->tab.textlen = 0;
      const char *str = term->title.last;
      while (*str && term->tab.textlen < TAB_MAX_TEXT_LEN) {
        cp = yterm_utf8d_consume(cp, *str); str += 1;
        if (yterm_utf8_valid_cp(cp)) {
          if (!yterm_utf8_printable_cp(cp)) cp = YTERM_UTF8_REPLACEMENT_CP;
          term->tab.text[term->tab.textlen].byte1 = (cp>>8)&0xffU;
          term->tab.text[term->tab.textlen].byte2 = cp&0xffU;
          term->tab.textlen += 1;
        }
      }
      term->tab.textwidth = XTextWidth16(x11_tabsfont.fst, term->tab.text, term->tab.textlen);
      // setup other args
      term->tab.width = tabw;
      term->tab.posx = posx;
      // advance
      tnum += 1;
      posx += tabw;
    }
  }

  (void)make_current_tab_visible();

  repaint_tabs(0, winWidth);
}
