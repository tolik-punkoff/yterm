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
  low-level X11 rendering (render cell buffer).
  included directly into the main file.
*/

// passive, active
enum {
  OSD_CSCHEME_ACTIVE = 1u<<0,
  OSD_CSCHEME_CURSOR = 1u<<1,
  OSD_CSCHEME_HOTKEY = 1u<<2,
};

// colors:
//   0 (0b_000): normal, passive
//   1 (0b_001): normal, active
//   2 (0b_010): cursor, passive
//   3 (0b_011): cursor, active
//   4 (0b_100): hotkey, passive
//   5 (0b_101): hotkey, active
//   6 (0b_110): hotkey, cursor, passive
//   7 (0b_111): hotkey, cursor, active

static uint32_t osd_menu_bg[8] = {
  0xa0a080, // normal, passive
  0xd9d9ae, // normal, active
  0x006666, // cursor, passive
  0x00bbbb, // cursor, active
  0xa0a080, // hotkey, passive
  0xd9d9ae, // hotkey, active
  0x006666, // hotkey, cursor, passive
  0x00bbbb, // hotkey, cursor, active
};

static uint32_t osd_menu_fg[8] = {
  0x000000, // normal, passive
  0x000000, // normal, active
  0x000000, // cursor, passive
  0x000000, // cursor, active
  0x660000, // hotkey, passive
  0x660000, // hotkey, active
  0x660000, // hotkey, cursor, passive
  0x660000, // hotkey, cursor, active
};


#define SET_FG_COLOR(clr_)  do { \
  const typeof(clr_)cc_ = (uint32_t)(clr_)&0xffffffU; \
  if (cc_ != lastFG) { \
    lastFG = cc_; \
    XSetForeground(x11_dpy, x11_gc, (unsigned long)cc_); \
  } \
} while (0)

#define SET_BG_COLOR(clr_)  do { \
  const typeof(clr_)cc_ = (uint32_t)(clr_)&0xffffffU; \
  if (cc_ != lastBG) { \
    lastBG = cc_; \
    XSetBackground(x11_dpy, x11_gc, (unsigned long)cc_); \
  } \
} while (0)


#define RENDER_SET_COLORS()  do { \
  if (lastBG != bg) { \
    lastBG = bg; \
    XSetBackground(x11_dpy, x11_gc, lastBG); \
  } \
  if (lastFG != fg) { \
    lastFG = fg; \
    XSetForeground(x11_dpy, x11_gc, lastFG); \
  } \
} while (0)


//  TODO: optimise blanks.
//        if we have many blanks with the same bg attr, draw a simple
//        rectangle instead of sending them char-by-char.
#define RENDER_FLUSH_STR()  do { \
  if (written != 0) { \
    XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc, winx, winy, tmpbuf, written); \
    /* draw underline */ \
    if (under) { \
      const int uy = winy + (x11_font.descent ? 1 : 0); \
      XDrawLine(x11_dpy, (Drawable)x11_win, x11_gc, winx, uy, winx + charWidth * written - 1, uy); \
    } \
    winx += written * charWidth; \
    written = 0; \
  } \
} while (0)

#define RENDER_FLUSH()  do { \
  RENDER_FLUSH_STR(); \
  RENDER_SET_COLORS(); under = uu; \
} while (0)


#define RENDER_ADVCHAR()  do { \
  uint32_t cp = (uint32_t)src->ch; \
  if (!yterm_utf8_printable_cp(cp)) cp = YTERM_UTF8_REPLACEMENT_CP; \
  tmpbuf[written].byte1 = (cp>>8)&0xffU; \
  tmpbuf[written].byte2 = cp&0xffU; \
  written += 1; \
  if (src->flags & CELL_DIRTY) { \
    src->flags ^= CELL_DIRTY; \
    cbuf->dirtyCount -= 1; \
    yterm_assert(cbuf->dirtyCount >= 0); \
  } \
  src += 1; count -= 1; cx += 1; \
} while (0)


static XChar2b *line_draw_buffer = NULL;
static size_t line_draw_buffer_size = 0;


//==========================================================================
//
//  renderLine
//
//  TODO: optimise blanks.
//        if we have many blanks with the same bg attr, draw a simple
//        rectangle instead of sending them char-by-char.
//
//==========================================================================
static void renderLine (CellBuffer *cbuf, const CursorPos *cpos, int cx, int cy, int count) {
  if (count <= 0 ||
      cy < 0 || cy >= cbuf->height ||
      (cx < 0 && cx + count <= 0) || cx >= cbuf->width)
  {
    return;
  }

  if (cx < 0) {
    count += cx; cx = 0;
    yterm_assert(count > 0);
  }
  if (cx + count > cbuf->width) {
    count = cbuf->width - cx;
    yterm_assert(count > 0);
  }

  int winx = cx * charWidth;
  const int winy = cy * charHeight + x11_font.ascent;

  // ensure line buffer
  const size_t nsz = (size_t)cbuf->width * sizeof(line_draw_buffer[0]) * 2;
  if (nsz > line_draw_buffer_size) {
    XChar2b *xss = realloc(line_draw_buffer, nsz);
    if (xss == NULL) abort();
    line_draw_buffer = xss;
    line_draw_buffer_size = nsz;
  }

  XChar2b *tmpbuf = line_draw_buffer;

  int curx = (cpos != NULL && cpos->y == cy ? cpos->x : -666);

  CharCell *src = cbuf->buf + cy * cbuf->width + cx;
  yterm_bool under = -1; // oops
  int written = 0;
  while (count > 0) {
    uint32_t fg, bg;
    yterm_bool uu;
    cbuf_decode_attrs(src, &fg, &bg, &uu);
    if (cx == curx) {
      // render cursor
      RENDER_FLUSH();
      if (winFocused && ((curColorsBG[curPhase] | curColorsFG[curPhase]) & 0xff000000U) == 0) {
        yterm_assert(written == 0);
        RENDER_ADVCHAR();
        if ((curColorsBG[curPhase] & 0xff000000U) == 0) {
          SET_BG_COLOR(curColorsBG[curPhase]);
        } else {
          SET_BG_COLOR(bg);
        }
        if ((curColorsFG[curPhase] & 0xff000000U) == 0) {
          SET_FG_COLOR(curColorsFG[curPhase]);
        } else {
          SET_FG_COLOR(fg);
        }
        RENDER_FLUSH();
      } else if (!winFocused && (curColorsBG[2] & 0xff000000) == 0) {
        RENDER_ADVCHAR();
        SET_BG_COLOR(bg);
        SET_FG_COLOR(fg);
        RENDER_FLUSH();
        // and draw a rect
        SET_FG_COLOR(curColorsBG[2]);
        #if 0
        if (charWidth > 3 && charHeight > 3) {
          XDrawRectangle(x11_dpy, (Drawable)x11_win, x11_gc,
                         winx - charWidth + 1, winy - x11_font.ascent + 1,
                         charWidth - 3, charHeight - 3);
        }
        #endif
        if (charWidth > 1 && charHeight > 1) {
          XDrawRectangle(x11_dpy, (Drawable)x11_win, x11_gc,
                         winx - charWidth, winy - x11_font.ascent,
                         charWidth - 1, charHeight - 1);
        }
      } else {
        curx = -666;
      }
    } else if (lastFG != fg || lastBG != bg || under != uu) {
      RENDER_FLUSH();
    } else {
      RENDER_ADVCHAR();
    }
  }
  RENDER_FLUSH_STR();
}


//==========================================================================
//
//  renderArea
//
//  used in expose events
//
//==========================================================================
static void renderArea (CellBuffer *cbuf, const CursorPos *cpos,
                        int winl, int wint, int winr, int winb)
{
  if (winr <= 0 || winb <= 0 ||
      winl >= winWidth || wint >= winHeight)
  {
    return;
  }

  // clamp
  if (winl < 0) winl = 0;
  if (wint < 0) wint = 0;
  if (winr > winWidth) winr = winWidth;
  if (winb > winHeight) winb = winHeight;
  if (winl >= winr || wint >= winb) return;

  const int ex = (winr - 1) / charWidth, ey = (winb - 1) / charWidth;
  const int cx = winl / charWidth;
  const int count = ex - cx + 1;
  int cy = wint / charHeight;

  while (cy <= ey) {
    renderLine(cbuf, cpos, cx, cy, count);
    cy += 1;
  }

  // erase right unused part
  if (ex >= cbuf->width) {
    SET_FG_COLOR(0);
    XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc, ex * charWidth, wint,
                   winr - ex * charWidth, winb - wint);
  }

  // erase bottom unused part
  if (ey >= cbuf->height) {
    SET_FG_COLOR(0);
    XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc, winl, ey * charHeight,
                   winr - winl, winb - ey * charHeight);
  }
}


//==========================================================================
//
//  renderDirtyLine
//
//  render all dirty chars in this line
//
//==========================================================================
static void renderDirtyLine (CellBuffer *cbuf, const CursorPos *cpos, int cy) {
  if (cy < 0 || cy >= cbuf->height) return;

  const int wdt = cbuf->width;

  CharCell *src = &cbuf->buf[cy * wdt];

  int rsx = 0; // render start x
  int rct = 0; // render count
  int ecc = 0; // empty cells count
  int cx = 0;
  while (cx < wdt) {
    if ((src->flags & CELL_DIRTY) == 0) {
      // unchanged cell
      //HACK: if we have several unchanged cells between two strips,
      //      include them in the strip. it helps with checkerboard
      //      patterns. they shouldn't happen often, but why not?
      if (rct) {
        ecc += 1;
        if (ecc > 3) {
          // too many unchanged cells, flush collected strip
          renderLine(cbuf, cpos, rsx, cy, rct);
          rct = 0;
        }
      }
    } else {
      // changed cell, append it to the strip
      if (!rct) rsx = cx; // new strip
      rct = cx - rsx + 1;
    }
    cx += 1;
    src += 1;
  }
  // render last strip
  if (rct) renderLine(cbuf, cpos, rsx, cy, rct);
}


//==========================================================================
//
//  renderDirty
//
//  render all dirty chars
//
//==========================================================================
static void renderDirty (CellBuffer *cbuf, const CursorPos *cpos) {
  int cy = 0;
  while (cy < cbuf->height && cbuf->dirtyCount != 0) {
    renderDirtyLine(cbuf, cpos, cy);
    cy += 1;
  }
  yterm_assert(cbuf->dirtyCount == 0);
}


static XChar2b *osd_draw_buffer = NULL;
static size_t osd_draw_buffer_size = 0;


//==========================================================================
//
//  osd_draw_ensure_buffer
//
//==========================================================================
static void osd_draw_ensure_buffer (int size) {
  if (size > 0) {
    const size_t nsz = (unsigned)size * sizeof(osd_draw_buffer[0]);
    if (nsz > osd_draw_buffer_size) {
      XChar2b *xss = realloc(osd_draw_buffer, nsz);
      yterm_assert(xss != NULL); // FIXME: handle this!
      osd_draw_buffer = xss;
      osd_draw_buffer_size = nsz;
    }
  }
}


//==========================================================================
//
//  osd_put_char
//
//==========================================================================
YTERM_STATIC_INLINE void osd_put_char (int x, uint32_t cp) {
  if (x >= 0 && (unsigned)x < osd_draw_buffer_size / sizeof(osd_draw_buffer[0])) {
    if (cp > 0xffff) cp = 0x20;
    osd_draw_buffer[x].byte1 = (cp>>8)&0xffU;
    osd_draw_buffer[x].byte2 = cp&0xffU;
  }
}


//==========================================================================
//
//  osd_copy_char2
//
//==========================================================================
YTERM_STATIC_INLINE void osd_copy_char2 (int x, const XChar2b *cp) {
  if (cp != NULL && x >= 0 && (unsigned)x < osd_draw_buffer_size / sizeof(osd_draw_buffer[0])) {
    osd_draw_buffer[x] = *cp;
  }
}


//==========================================================================
//
//  XCreateBoxRegion
//
//==========================================================================
static Region XCreateBoxRegion (int x, int y, int w, int h) {
  Region reg = XCreateRegion();
  if (w > 0 && h > 0) {
    XRectangle xrect;
    xrect.x = x; xrect.y = y;
    xrect.width = w; xrect.height = h;
    Region xreg = XCreateRegion();
    XUnionRectWithRegion(&xrect, xreg, reg);
    XDestroyRegion(xreg);
  }
  return reg;
}


//==========================================================================
//
//  XCutBoxRegion
//
//==========================================================================
static Region XCutBoxRegion (Region reg, int x, int y, int w, int h) {
  if (w > 0 && h > 0) {
    Region box = XCreateBoxRegion(x, y, w, h);
    Region dest = XCreateRegion();
    XSubtractRegion(reg, box, dest);
    XDestroyRegion(reg);
    return dest;
  } else {
    return reg;
  }
}


//==========================================================================
//
//  x11_render_osd
//
//==========================================================================
static Region x11_render_osd (const char *msg, Region clipreg) {
  Term *term = currterm;
  if (term == NULL || msg == NULL || msg[0] == 0) return clipreg;

  osd_draw_ensure_buffer((int)strlen(msg));

  const int winx = 0;
  int winy = 0;
  int s16len = 0;
  uint32_t cp = 0;
  while (*msg && s16len < TERM_CBUF(term)->width) {
    cp = yterm_utf8d_consume(cp, *msg); msg += 1;
    if (yterm_utf8_valid_cp(cp)) {
      if (!yterm_utf8_printable_cp(cp)) cp = YTERM_UTF8_REPLACEMENT_CP;
      osd_put_char(s16len, cp);
      s16len += 1;
    }
  }

  if (s16len != 0) {
    SET_BG_COLOR(0xff7f00);
    SET_FG_COLOR(0x000000);
    winy += x11_font.ascent;
    XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc, winx, winy, osd_draw_buffer, s16len);
    // clip message area out
    clipreg = XCutBoxRegion(clipreg, 0, 0, s16len * charWidth, charHeight);
  }

  return clipreg;
}


// frame chars:
// left and right vertical bars
// top and bottom horizontal bars
// left and right frame chars
// corners, from the left top, clockwise
// submenu char
// checkbox: empty, checked, unknown
// solid horizontal line
static const uint16_t menu_frame_chars[15] = {
  #if 0
  // squared
  0x2502, 0x2502, // left and right vertical bars
  0x2500, 0x2500, // top and bottom horizontal bars
  0x2524, 0x251c, // left and right frame chars
  0x250c, 0x2510, 0x2514, 0x2518, // corners, from the left top, clockwise
  #else
  // rounded
  0x2502, 0x2502, // left and right vertical bars
  0x2500, 0x2500, // top and bottom horizontal bars
  0x2574, 0x2576, // left and right frame chars
  0x256d, 0x256e, 0x2570, 0x256f, // corners, from the left top, clockwise
  #endif
  0x25ba, // submenu char
  0x0020, 0x2713, '?', // checkbox: empty, checked, unknown
  0x2500, // solid horizontal line
  /*
  ╭─╴tt╶─╮
  │      │
  ╰──────╯
  */
  // ✓✔✗✘
  // ●■ ◊○ ◆ ■▮▬
  // ☺☻♥♦♣♠•◘○ ▲▼◆
  // ▲▶►▼◀◄
  /*
  U+250x ─ ━ │ ┃ ┄ ┅ ┆ ┇ ┈ ┉ ┊ ┋ ┌ ┍ ┎ ┏
  U+251x ┐ ┑ ┒ ┓ └ ┕ ┖ ┗ ┘ ┙ ┚ ┛ ├ ┝ ┞ ┟
  U+252x ┠ ┡ ┢ ┣ ┤ ┥ ┦ ┧ ┨ ┩ ┪ ┫ ┬ ┭ ┮ ┯
  U+253x ┰ ┱ ┲ ┳ ┴ ┵ ┶ ┷ ┸ ┹ ┺ ┻ ┼ ┽ ┾ ┿
  U+254x ╀ ╁ ╂ ╃ ╄ ╅ ╆ ╇ ╈ ╉ ╊ ╋ ╌ ╍ ╎ ╏
  U+255x ═ ║ ╒ ╓ ╔ ╕ ╖ ╗ ╘ ╙ ╚ ╛ ╜ ╝ ╞ ╟
  U+256x ╠ ╡ ╢ ╣ ╤ ╥ ╦ ╧ ╨ ╩ ╪ ╫ ╬ ╭ ╮ ╯
  U+257x ╰ ╱ ╲ ╳ ╴ ╵ ╶ ╷ ╸ ╹ ╺ ╻ ╼ ╽ ╾ ╿
  */
};


//==========================================================================
//
//  x11_render_menu_cbox_char
//
//==========================================================================
static uint32_t x11_render_menu_cbox_char (MenuItem *it) {
  uint32_t res = 0;
  if (it != NULL) {
    int val = -1;
    switch (it->type) {
      case MENU_BOOL:
        if (it->varptr != NULL) val = *(yterm_bool *)it->varptr;
        goto draw_cbox;
      case MENU_3BOOL:
        if (it->varptr != NULL) val = *(int *)it->varptr;
        goto draw_cbox;
      case MENU_BITBOOL32:
        if (it->varptr != NULL) val = ((*(int *)it->varptr) & it->mask ? 1 : 0);
       draw_cbox:
        res = (val < 0 ? menu_frame_chars[13]
                       : val == 0 ? menu_frame_chars[11]
                       : menu_frame_chars[12]);
        break;
    }
  }
  return res;
}


//==========================================================================
//
//  x11_render_menu_prepare_item
//
//==========================================================================
static void x11_render_menu_prepare_item (MenuWindow *menu, MenuItem *it,
                                          int *hotx, char *hotch)
{
  yterm_assert(menu != NULL);
  yterm_assert(it != NULL);
  yterm_assert(menu->width >= 2);

  *hotx = -1; *hotch = 0;
  int xpos = 2;

  if (it->type == MENU_LABEL && it->caption.text == NULL) {
    // solid line
    for (int f = 0; f < menu->width; f += 1) osd_put_char(f, menu_frame_chars[14]);

    // frame
    //osd_put_char(1, 0x20);
    //osd_put_char(menu->width - 2, 0x20);
    osd_put_char(menu->width - 1, menu_frame_chars[0]);
    osd_put_char(0, menu_frame_chars[1]);
  } else {
    for (int f = 0; f < menu->width; f += 1) osd_put_char(f, 0x20);

    // checkbox
    uint32_t cbc = x11_render_menu_cbox_char(it);
    if (cbc != 0) {
      osd_put_char(xpos+0, '[');
      osd_put_char(xpos+1, cbc);
      osd_put_char(xpos+2, ']');
      osd_put_char(xpos+3, ' ');
      xpos += 4;
    }

    // text
    for (int f = 0; f < it->caption.len; f += 1) {
      if (xpos < menu->width - 2) {
        osd_copy_char2(xpos, &it->caption.text[f]);
      }
      if (f == it->caption.hotpos) {
        *hotx = xpos;
        *hotch = (char)it->caption.text[f].byte2;
      }
      xpos += 1;
    }

    if (it->type == MENU_SUBMENU) {
      osd_put_char(menu->width - 3, menu_frame_chars[10]);
    }

    // frame
    osd_put_char(1, 0x20);
    osd_put_char(menu->width - 2, 0x20);
    osd_put_char(menu->width - 1, menu_frame_chars[0]);
    osd_put_char(0, menu_frame_chars[1]);
  }
}


//==========================================================================
//
//  x11_render_osd_menu_window
//
//  also modifies the clip region (and sets it)
//
//==========================================================================
static Region x11_render_osd_menu_window (MenuWindow *menu, yterm_bool top, Region clipreg) {
  if (menu == NULL) return clipreg;

  const int winx = menu->x0 * charWidth;
  int winy = menu->y0 * charHeight;

  if (menu->dirty) {
    XSetRegion(x11_dpy, x11_gc, clipreg);
  }

  // modify clip region
  clipreg = XCutBoxRegion(clipreg, winx, winy,
                          menu->width * charWidth, menu->height * charHeight);

  if (menu->dirty) {
    menu->dirty = 0;

    if ((opt_osd_menu_shadow_color & 0xff000000U) == 0) {
      // draw menu shadow, and cut it from the region
      SET_FG_COLOR(opt_osd_menu_shadow_color);
      // right part
      XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc,
                     winx + menu->width * charWidth, winy + opt_osd_menu_shadow_yofs,
                     opt_osd_menu_shadow_xofs, menu->height * charHeight - opt_osd_menu_shadow_yofs);
      clipreg = XCutBoxRegion(clipreg,
                              winx + menu->width * charWidth, winy + opt_osd_menu_shadow_yofs,
                              opt_osd_menu_shadow_xofs, menu->height * charHeight - opt_osd_menu_shadow_yofs);
      // bottom part
      XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc,
                     winx + opt_osd_menu_shadow_xofs, winy + menu->height * charHeight,
                     menu->width * charWidth, opt_osd_menu_shadow_yofs);
      clipreg = XCutBoxRegion(clipreg,
                              winx + opt_osd_menu_shadow_xofs, winy + menu->height * charHeight,
                              menu->width * charWidth, opt_osd_menu_shadow_yofs);
    }

    // we will never draw more than this
    osd_draw_ensure_buffer(menu->width);

    // color scheme
    unsigned osd_cscheme = (top ? OSD_CSCHEME_ACTIVE : 0u);

    // just in case
    if (menu->width < 3 || menu->height < 2) {
      SET_FG_COLOR(osd_menu_bg[osd_cscheme]);
      if (menu->width > 0 && menu->height > 0) {
        XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc, winx, winy,
                       menu->width * charWidth, menu->height * charHeight);
      }
      return clipreg;
    }

    SET_FG_COLOR(osd_menu_fg[osd_cscheme]);
    SET_BG_COLOR(osd_menu_bg[osd_cscheme]);

    // top line: title
    for (int f = 0; f < menu->width; f += 1) osd_put_char(f, menu_frame_chars[2]);
    int txpos = (menu->width - menu->title.len) / 2;
    for (int f = 0; f < menu->title.len; f += 1) {
      osd_copy_char2(txpos + f, &menu->title.text[f]);
    }
    if (txpos >= 2 && txpos + menu->title.len <= menu->width - 2) {
      osd_put_char(txpos - 1, menu_frame_chars[4]);
      osd_put_char(txpos + menu->title.len, menu_frame_chars[5]);
    }
    osd_put_char(menu->width - 1, menu_frame_chars[7]);
    osd_put_char(0, menu_frame_chars[6]);
    XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc,
                       winx, winy + x11_font.ascent,
                       osd_draw_buffer, menu->width);

    // draw items
    MenuItem *it = menu->items;
    int left = menu->top_item;
    while (left != 0 && it != NULL) { it = it->next; left -= 1; }

    winy += charHeight;
    int ity = 1;
    int cidx = menu->top_item;
    int hotx; char hotch;
    while (ity < menu->height - 1 && it != NULL) {
      x11_render_menu_prepare_item(menu, it, &hotx, &hotch);

      unsigned curflag = 0;
      if (cidx == menu->curr_item && menu_is_enabled(it)) {
        curflag = OSD_CSCHEME_CURSOR;
        // need to render left and right frame separately
        XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc,
                           winx, winy + x11_font.ascent,
                           osd_draw_buffer, 1);
        XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc,
                           winx + (menu->width - 1) * charWidth, winy + x11_font.ascent,
                           osd_draw_buffer + menu->width - 1, 1);
        SET_BG_COLOR(osd_menu_bg[osd_cscheme | curflag]);
        SET_FG_COLOR(osd_menu_fg[osd_cscheme | curflag]);
        XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc,
                           winx + charWidth, winy + x11_font.ascent,
                           osd_draw_buffer + 1, menu->width - 2);
        SET_BG_COLOR(osd_menu_bg[osd_cscheme]);
        SET_FG_COLOR(osd_menu_fg[osd_cscheme]);
      } else {
        // can draw the whole line
        XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc,
                           winx, winy + x11_font.ascent,
                           osd_draw_buffer, menu->width);
      }

      if (hotx != -1) {
        SET_BG_COLOR(osd_menu_bg[osd_cscheme | curflag | OSD_CSCHEME_HOTKEY]);
        SET_FG_COLOR(osd_menu_fg[osd_cscheme | curflag | OSD_CSCHEME_HOTKEY]);
        XDrawString(x11_dpy, (Drawable)x11_win, x11_gc,
                    winx + hotx * charWidth, winy + x11_font.ascent,
                    &hotch, 1);
        SET_BG_COLOR(osd_menu_bg[osd_cscheme]);
        SET_FG_COLOR(osd_menu_fg[osd_cscheme]);
      }

      cidx += 1; ity += 1; winy += charHeight;
      it = it->next;
    }

    while (ity < menu->height - 1) {
      for (int f = 0; f < menu->width; f += 1) osd_put_char(f, 0x20);
      // frame
      osd_put_char(menu->width - 1, menu_frame_chars[0]);
      osd_put_char(0, menu_frame_chars[1]);
      XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc,
                         winx, winy + x11_font.ascent,
                         osd_draw_buffer, menu->width);
      ity += 1; winy += charHeight;
    }

    // bottom line
    for (int f = 0; f < menu->width; f += 1) osd_put_char(f, 0x2500);
    osd_put_char(menu->width - 1, menu_frame_chars[9]);
    osd_put_char(0, menu_frame_chars[8]);
    XDrawImageString16(x11_dpy, (Drawable)x11_win, x11_gc,
                       winx, winy + x11_font.ascent,
                       osd_draw_buffer, menu->width);
  }

  return clipreg;
}


//==========================================================================
//
//  x11_render_osd_menus
//
//==========================================================================
static Region x11_render_osd_menus (MenuWindow *menu) {
  // do not draw over the tabbar
  Region reg = XCreateBoxRegion(0, 0, winWidth, winHeight);
  if (menu != NULL) {
    yterm_bool top = 1;
    while (menu != NULL) {
      reg = x11_render_osd_menu_window(menu, top, reg);
      top = 0; menu = menu->prev;
    }
  }
  return reg;
}
