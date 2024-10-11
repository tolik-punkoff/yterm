/*
 * SXED -- sexy text editor engine, 2022
 *
 * Config directories
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
#include "cellwin.h"
#include "utf.h"

#include <wchar.h>
#include <unistd.h>


// ////////////////////////////////////////////////////////////////////////// //
//#define SLOW_DIRTY_CHECKS
//#define SCROLL_DIRTY_CHECKS

#ifdef SLOW_DIRTY_CHECKS
# define CHECK_DIRTY(cbf_)  do { check_dirty_count(cbf_, __LINE__); } while (0)
#else
# define CHECK_DIRTY(cbf_)  ((void)0)
#endif


// ////////////////////////////////////////////////////////////////////////// //
#define IS_DIRTY(cell_)  ((cell_)->flags & CELL_DIRTY)

#define CLIP_REGION()  \
  if (cbuf == NULL) return; \
  if (x1 < 0 || y1 < 0 || x0 > x1 || y0 > y1) return; \
  const int cbwdt = cbuf->width; \
  if (x0 >= cbwdt) return; \
  const int cbhgt = cbuf->height; \
  if (y0 >= cbhgt) return; \
  if (x0 < 0) x0 = 0; \
  if (y0 < 0) y0 = 0; \
  if (x1 >= cbwdt) x1 = cbwdt - 1; \
  if (y1 >= cbhgt) y1 = cbhgt - 1; \
  yterm_assert(x0 <= x1 && y0 <= y1); \
  do {} while (0);

#define LOAD_ATTRS()  \
  uint16_t flags; \
  uint32_t fg, bg; \
  do { \
    if (attr != NULL) { \
      flags = (attr->flags | CELL_DIRTY) & (~CELL_AUTO_WRAP); \
      fg = attr->fg; bg = attr->bg; \
    } else { \
      flags = CELL_DIRTY; \
      fg = CELL_ATTR_MASK | CELL_DEFAULT_ATTR; \
      bg = CELL_ATTR_MASK | CELL_DEFAULT_ATTR; \
    } \
  } while (0)

#define DIFF_CELL(cc_,ch_)  (\
  ((cc_)->ch ^ (uint32_t)(ch_)) | \
  ((cc_)->fg ^ fg) | \
  ((cc_)->bg ^ bg) | \
  (((cc_)->flags ^ flags) & ~CELL_AUTO_WRAP) \
)

#define DIFF_TWO_CELLS(cc0_,cc1_)  (\
  ((cc0_)->ch ^ (cc1_)->ch) | \
  ((cc0_)->fg ^ (cc1_)->fg) | \
  ((cc0_)->bg ^ (cc1_)->bg) | \
  (((cc0_)->flags ^ (cc1_)->flags) & ~(CELL_AUTO_WRAP|CELL_DIRTY)) \
)


// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  int w, h;
  int x, y;
  int addr;
  CharCell *buf;
} LineIt;


// ////////////////////////////////////////////////////////////////////////// //
int colorMode = CMODE_NORMAL;
yterm_bool enableUnderline = 1;
yterm_bool cbufInverse = 0;


// ////////////////////////////////////////////////////////////////////////// //
uint32_t colorsTTY[CIDX_MAX + 1];
uint32_t colorSelectionFG = 0xffffff;
uint32_t colorSelectionBG = 0x0055cc;
uint32_t colorAmberFG = 0xffaa00;
uint32_t colorAmberBG = 0x333333;


//==========================================================================
//
//  cbuf_init_palette
//
//  init default palette
//
//==========================================================================
void cbuf_init_palette (void) {
  const uint32_t defclr16 [16] = {
    // 8 normal colors
    0x000000,
    0xb21818,
    0x18b218,
    0xb26818,
    0x1818b2,
    0xb218b2,
    0x18b2b2,
    0xb2b2b2,
    // 8 bright colors
    0x686868,
    0xff5454,
    0x54ff54,
    0xffff54,
    0x5454ff,
    0xff54ff,
    0x54ffff,
    0xffffff,
  };

  memcpy(colorsTTY, defclr16, 16 * sizeof(colorsTTY[0]));

  colorsTTY[CIDX_DEFAULT_FG] = 0xb2b2b2;
  colorsTTY[CIDX_DEFAULT_BG] = 0x000000;
  colorsTTY[CIDX_DEFAULT_HIGH_FG] = 0xffffff;
  colorsTTY[CIDX_DEFAULT_HIGH_BG] = 0x000000;

  colorsTTY[CIDX_BOLD_FG] = 0x00afaf;
  colorsTTY[CIDX_UNDER_FG] = 0x00af00;
  colorsTTY[CIDX_BOLD_UNDER_FG] = 0xafaf00;

  colorsTTY[CIDX_BOLD_HIGH_FG] = 0x00ffff;
  colorsTTY[CIDX_UNDER_HIGH_FG] = 0x44ff00;
  colorsTTY[CIDX_BOLD_UNDER_HIGH_FG] = 0xffff44;

  // xterm 256 color palette
  int cidx = 16;
  for (uint32_t r = 0; r < 6; r += 1) {
    for (uint32_t g = 0; g < 6; g += 1) {
      for (uint32_t b = 0; b < 6; b += 1) {
        uint32_t c = 0;
        if (r) c += (0x37 + 0x28 * r) << 16;
        if (g) c += (0x37 + 0x28 * g) << 8;
        if (b) c += (0x37 + 0x28 * b);
        colorsTTY[cidx] = c;
        cidx += 1;
      }
    }
  }
  yterm_assert(cidx == 232);
  // 24 grayscale colors
  for (uint32_t gs = 0; gs < 24; gs += 1) {
    uint32_t c = 0x08 + 0x0a * gs;
    c = c | (c << 8) | (c << 16);
    colorsTTY[cidx] = c;
    cidx += 1;
  }
  yterm_assert(cidx == 256);
}


//==========================================================================
//
//  check_dirty_count
//
//==========================================================================
static __attribute__((unused)) void check_dirty_count (const CellBuffer *cbuf, const int line) {
  if (cbuf) {
    int dc = 0;
    const CharCell *cc = cbuf->buf;
    int cnt = cbuf->width * cbuf->height;
    while (cnt != 0) {
      cnt -= 1;
      if (IS_DIRTY(cc)) dc += 1;
      cc += 1;
    }
    if (dc != cbuf->dirtyCount) {
      fprintf(stderr, "DIRTY COUNT ERROR at line %d: calc=%d, stored=%d\n",
              line, dc, cbuf->dirtyCount);
      yterm_assert(0);
    }
  }
}


//==========================================================================
//
//  it_init
//
//==========================================================================
YTERM_STATIC_INLINE void it_init (LineIt *it, CellBuffer *cbuf) {
  it->w = cbuf->width;
  it->h = cbuf->height;
  it->x = 0;
  it->y = 0;
  it->addr = 0;
  it->buf = cbuf->buf;
}


//==========================================================================
//
//  it_is_eot
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool it_is_eot (const LineIt *it) {
  return (it->y == it->h);
}


//==========================================================================
//
//  it_get
//
//  get next char
//  returns `NULL` on end of buffer, or on EOL
//
//==========================================================================
YTERM_STATIC_INLINE const CharCell *it_get (LineIt *it) {
  const CharCell *res = NULL;
  if (it->y != it->h) {
    if (it->x == it->w) {
      it->x = 0; it->y += 1;
    } else {
      res = &it->buf[it->addr];
      it->x += 1; it->addr += 1;
      // if last in line, check for autowraping
      if (it->x == it->w && (res->flags & CELL_AUTO_WRAP) != 0) {
        // autowrap, move to the next line
        it->x = 0; it->y += 1;
      }
    }
  }
  return res;
}


//==========================================================================
//
//  decode_color
//
//==========================================================================
YTERM_STATIC_INLINE uint32_t decode_color (uint32_t c, uint16_t flags,
                                           yterm_bool *under, yterm_bool isfg)
{
  if (c & CELL_ATTR_MASK) {
    // predefined terminal colors
    c &= ~CELL_ATTR_MASK;
    if (c > 255) c = CIDX_DEFAULT_BG - isfg; // default attribute

    yterm_bool high = ((flags & CELL_BLINK) != 0); // high intensity?

    if (isfg) {
      if ((flags & CELL_BOLD) != 0) { // bold
        // either high intensity color, or special bold color
        if (c > 255) {
          // default color, transform to special
          if ((flags & CELL_UNDERLINE) != 0) {
            c = CIDX_BOLD_UNDER_FG + 3 * high;
          } else {
            c = CIDX_BOLD_FG + 3 * high;
          }
        } else {
          // non-default color
          high = 1;
          if (c >= 8 && enableUnderline && (flags & CELL_UNDERLINE) != 0) {
            *under = 1;
          }
        }
      } else if ((flags & CELL_UNDERLINE) != 0) { // underline?
        if (c > 255) {
          // default color, transform to special
          c = CIDX_UNDER_FG + 3 * high;
        } else {
          if (enableUnderline) *under = 1;
        }
      }
    }

    if (high) {
      // high intensity
      if (c < 8) c += 8;
      else if (c == CIDX_DEFAULT_FG || c == CIDX_DEFAULT_BG) c += 2;
    }

    yterm_assert(c <= CIDX_MAX);
    c = colorsTTY[c];
  }

  if (colorMode == CMODE_AMBER) {
    c = (isfg ? colorAmberFG : colorAmberBG);
  } else if (colorMode == CMODE_BW || colorMode == CMODE_GREEN || colorMode == CMODE_AMBER_TINT) {
    //0.3*r + 0.59*g + 0.11*b
    int lumi = (19660 * ((c>>16)&0xff) +
                38666 * ((c>>8)&0xff) +
                7208 * (c&0xff)) >> 16;
    if (lumi > 255) lumi = 255;
    if (colorMode == CMODE_BW) {
      c = ((uint32_t)lumi << 16) | ((uint32_t)lumi << 8) | (uint32_t)lumi;
    } else if (colorMode == CMODE_GREEN) {
      c = ((uint32_t)lumi << 8);
    } else {
      // amber tint
      lumi = 1 - ((lumi >> 7) & 0x01);
      uint32_t xclr = (isfg ? colorAmberFG : colorAmberBG);
      c = ((((xclr >> 16) & 0xff) >> lumi) << 16) |
          ((((xclr >> 8) & 0xff) >> lumi) << 8) |
          ((xclr & 0xff) >> lumi);
    }
  }

  return c;
}


//==========================================================================
//
//  cbuf_decode_attrs
//
//==========================================================================
YTERM_PUBLIC void cbuf_decode_attrs (const CharCell *cc, uint32_t *fg, uint32_t *bg,
                                     yterm_bool *under)
{
  *under = 0;
  if ((cc->flags & CELL_SELECTION) == 0) {
    if (!(cc->flags & CELL_INVERSE) == !cbufInverse) {
      *fg = decode_color(cc->fg, cc->flags, under, 1);
      *bg = decode_color(cc->bg, cc->flags, under, 0);
    } else {
      *bg = decode_color(cc->fg, cc->flags, under, 1);
      *fg = decode_color(cc->bg, cc->flags, under, 0);
    }
  } else {
    *fg = colorSelectionFG;
    *bg = colorSelectionBG;
  }
}


//==========================================================================
//
//  set_cell_defaults
//
//==========================================================================
YTERM_STATIC_INLINE void set_cell_defaults (CharCell *cell) {
  cell->ch = 0x20;
  cell->flags = CELL_DIRTY;
  cell->fg = CELL_ATTR_MASK | CELL_DEFAULT_ATTR;
  cell->bg = CELL_ATTR_MASK | CELL_DEFAULT_ATTR;
}


//==========================================================================
//
//  cbuf_set_cell_defaults
//
//==========================================================================
YTERM_PUBLIC void cbuf_set_cell_defaults (CharCell *cell) {
  if (cell != NULL) set_cell_defaults(cell);
}


//==========================================================================
//
//  cbuf_new
//
//==========================================================================
YTERM_PUBLIC void cbuf_new (CellBuffer *cbuf, int nwidth, int nheight) {
  yterm_assert(cbuf);
  yterm_assert(nwidth >= MinBufferWidth && nwidth <= MaxBufferWidth);
  yterm_assert(nheight >= MinBufferHeight && nheight <= MaxBufferHeight);
  memset((void *)cbuf, 0, sizeof(*cbuf));
  const int bsz = nwidth * nheight;
  cbuf->buf = malloc(sizeof(cbuf->buf[0]) * (size_t)bsz);
  yterm_assert(cbuf->buf);
  for (int f = 0; f < bsz; f += 1) {
    set_cell_defaults(&cbuf->buf[f]);
  }
  cbuf->width = nwidth;
  cbuf->height = nheight;
  cbuf->dirtyCount = bsz;
  CHECK_DIRTY(cbuf);
}


//==========================================================================
//
//  cbuf_free
//
//==========================================================================
YTERM_PUBLIC void cbuf_free (CellBuffer *cbuf) {
  if (cbuf != NULL) {
    if (cbuf->buf) free(cbuf->buf);
    memset((void *)cbuf, 0, sizeof(*cbuf));
  }
}


//==========================================================================
//
//  cbuf_resize
//
//==========================================================================
YTERM_PUBLIC void cbuf_resize (CellBuffer *cbuf, int nwidth, int nheight,
                               yterm_bool relayout)
{
  yterm_assert(nwidth >= MinBufferWidth && nwidth <= MaxBufferWidth);
  yterm_assert(nheight >= MinBufferHeight && nheight <= MaxBufferHeight);

  if (cbuf != NULL && (cbuf->width != nwidth || cbuf->height != nheight)) {
    const int bsz = nwidth * nheight;
    CharCell *nbuf = malloc(sizeof(cbuf->buf[0]) * (size_t)bsz);
    yterm_assert(nbuf);
    CharCell *dest = nbuf;
    cbuf->dirtyCount = 0;
    const CharCell *src;
    if (relayout) {
      // relayout lines
      LineIt it;
      int x = 0, y = 0;
      // write lines to the new buffer
      it_init(&it, cbuf);
      while (y < nheight) {
        src = it_get(&it);
        if (src != NULL) {
          if (x == nwidth) {
            if (y != 0) dest[-1].flags |= CELL_AUTO_WRAP;
            x = 0; y += 1;
            if (y == nheight) break;
          }
          *dest = *src;
          dest->flags &= ~CELL_AUTO_WRAP; // just in case
          dest->flags |= CELL_DIRTY;
          dest += 1; x += 1;
        } else {
          // end of line, or end of text
          while (x < nwidth) {
            set_cell_defaults(dest);
            dest += 1; x += 1;
          }
          x = 0; y += 1;
        }
      }
    } else {
      src = cbuf->buf;
      const int xmin = min2(cbuf->width, nwidth);
      for (int y = 0; y < nheight; y += 1) {
        if (y < cbuf->height) {
          memcpy(dest, src, (unsigned)xmin * sizeof(CharCell));
          for (int f = 0; f < xmin; f += 1) {
            dest[f].flags &= ~CELL_AUTO_WRAP; // just in case
            dest[f].flags |= CELL_DIRTY;
          }
          for (int f = xmin; f < nwidth; f += 1) set_cell_defaults(&dest[f]);
        } else {
          for (int f = 0; f < nwidth; f += 1) set_cell_defaults(&dest[f]);
        }
        src += cbuf->width;
        dest += nwidth;
      }
    }
    free(cbuf->buf);
    cbuf->buf = nbuf;
    cbuf->width = nwidth;
    cbuf->height = nheight;
    cbuf->dirtyCount = bsz;
    CHECK_DIRTY(cbuf);
  }
}


//==========================================================================
//
//  cbuf_mark_line_autowrap
//
//==========================================================================
YTERM_PUBLIC void cbuf_mark_line_autowrap (CellBuffer *cbuf, int y) {
  if (cbuf != NULL && y >= 0 && y < cbuf->height) {
    CharCell *cc = &cbuf->buf[(y + 1) * cbuf->width - 1];
    cc->flags |= CELL_AUTO_WRAP;
  }
}


//==========================================================================
//
//  cbuf_unmark_line_autowrap
//
//==========================================================================
YTERM_PUBLIC void cbuf_unmark_line_autowrap (CellBuffer *cbuf, int y) {
  if (cbuf != NULL && y >= 0 && y < cbuf->height) {
    CharCell *cc = &cbuf->buf[(y + 1) * cbuf->width - 1];
    cc->flags &= ~CELL_AUTO_WRAP;
  }
}


#define PROCESS_REGION(code_)  \
  CLIP_REGION(); \
  CharCell *cc = &cbuf->buf[y0 * cbwdt + x0]; \
  const int wdt = x1 - x0 + 1; \
  const int ofs = cbwdt - wdt; \
  while (y0 <= y1) { \
    int cnt = wdt; \
    while (cnt != 0) { \
      cnt -= 1; \
      code_ \
      cc += 1; \
    } \
    cc += ofs; \
    y0 += 1; \
  } \
  CHECK_DIRTY(cbuf); \
  do {} while (0)


//==========================================================================
//
//  cbuf_mark_region_dirty
//
//  region is inclusive
//
//==========================================================================
YTERM_PUBLIC void cbuf_mark_region_dirty (CellBuffer *cbuf, int x0, int y0, int x1, int y1) {
  PROCESS_REGION(
    if (!IS_DIRTY(cc)) {
      cbuf->dirtyCount += 1;
      cc->flags |= CELL_DIRTY;
    }
  );
}


//==========================================================================
//
//  cbuf_mark_region_clean
//
//==========================================================================
YTERM_PUBLIC void cbuf_mark_region_clean (CellBuffer *cbuf, int x0, int y0, int x1, int y1) {
  PROCESS_REGION(
    if (IS_DIRTY(cc)) {
      cbuf->dirtyCount -= 1;
      cc->flags &= ~CELL_DIRTY;
    }
  );
}


//==========================================================================
//
//  cbuf_clear_region
//
//  region is inclusive
//
//==========================================================================
YTERM_PUBLIC void cbuf_clear_region (CellBuffer *cbuf, int x0, int y0, int x1, int y1,
                                     const CharCell *attr)
{
  LOAD_ATTRS();
  const uint32_t clearch = (attr ? attr->ch : 0x02);
  PROCESS_REGION(
    if (DIFF_CELL(cc, clearch)) {
      if (!IS_DIRTY(cc)) cbuf->dirtyCount += 1;
      cc->ch = clearch;
      cc->flags = flags;
      cc->fg = fg;
      cc->bg = bg;
    }
  );
}


//==========================================================================
//
//  cbuf_merge_all_dirty
//
//  this is called on screen switching.
//  only non-dirty and totally unchanged cells will remain non-dirty.
//
//==========================================================================
YTERM_PUBLIC void cbuf_merge_all_dirty (CellBuffer *cbuf, const CellBuffer *sbuf) {
  if (cbuf == NULL || sbuf == NULL || cbuf == sbuf) return;
  const int wdt = min2(cbuf->width, sbuf->width);
  const int hgt = min2(cbuf->height, sbuf->height);
  const int slineadd = sbuf->width - wdt;
  const int dlineadd = cbuf->width - wdt;
  const CharCell *src = sbuf->buf;
  CharCell *dest = cbuf->buf;
  for (int y = 0; y < hgt; y += 1) {
    for (int x = 0; x < wdt; x += 1) {
      if (!IS_DIRTY(dest) && (IS_DIRTY(src) || DIFF_TWO_CELLS(src, dest))) {
        cbuf->dirtyCount += 1;
        dest->flags |= CELL_DIRTY;
      }
      src += 1; dest += 1;
    }
    src += slineadd;
    dest += dlineadd;
  }
}


//==========================================================================
//
//  cbuf_write_wchar_count
//
//==========================================================================
YTERM_PUBLIC void cbuf_write_wchar_count (CellBuffer *cbuf, int x, int y, uint32_t ch,
                                          int count, const CharCell *attr)
{
  if (cbuf == NULL || count < 1 || y < 0 || y >= cbuf->height ||
      x >= cbuf->width || (x < 0 && x + count <= 0))
  {
    return;
  }
  if (x < 0) { count += x; x = 0; }
  if (cbuf->width - x < count) count = cbuf->width - x;
  LOAD_ATTRS();
  CharCell *cc = &cbuf->buf[y * cbuf->width + x];
  if (ch > 0xffff) ch = 0xFFFDU; // invalid unicode
  while (count != 0) {
    count -= 1;
    if (DIFF_CELL(cc, ch)) {
      if (!IS_DIRTY(cc)) cbuf->dirtyCount += 1;
      cc->ch = ch;
      cc->flags = flags;
      cc->fg = fg;
      cc->bg = bg;
    }
    cc += 1;
  }
  CHECK_DIRTY(cbuf);
}


//==========================================================================
//
//  cbuf_write_utf
//
//==========================================================================
YTERM_PUBLIC void cbuf_write_utf (CellBuffer *cbuf, int x, int y, const char *str,
                                  const CharCell *attr)
{
  if (cbuf == NULL || str == NULL || str[0] == 0 ||
      y < 0 || x >= cbuf->width || y >= cbuf->height)
  {
    return;
  }
  const int wdt = cbuf->width;
  uint32_t cp = 0;
  CharCell *cc = &cbuf->buf[y * wdt] + (x > 0 ? x : 0);
  LOAD_ATTRS();
  while (*str && x < wdt) {
    cp = yterm_utf8d_consume(cp, *str); str += 1;
    if (yterm_utf8_valid_cp(cp)) {
      if (x >= 0) {
        if (DIFF_CELL(cc, cp)) {
          if (!IS_DIRTY(cc)) cbuf->dirtyCount += 1;
          cc->ch = cp;
          cc->flags = flags;
          cc->fg = fg;
          cc->bg = bg;
        }
        cc += 1;
      }
      x += 1;
    }
  }
  CHECK_DIRTY(cbuf);
}


//==========================================================================
//
//  copy_strip
//
//  copy continuous strip, with possible overlap
//  it can either merge dirty state, or overwrite
//  no input checking is done, it is caller's responsibility
//
//==========================================================================
static void copy_strip (CellBuffer *cbuf, int srcofs, int destofs, int count,
                        yterm_bool merge_dirty)
{
  yterm_assert(count > 0);
  const CharCell *src = &cbuf->buf[srcofs];
  CharCell *dest = &cbuf->buf[destofs];
  ssize_t dir;
  if (srcofs > destofs) {
    // forward copy
    dir = 1;
  } else {
    // backward copy
    src += count - 1;
    dest += count - 1;
    dir = -1;
  }
  // now do it; use pasta for small speed gain
  if (merge_dirty) {
    // merge dirty flags
    while (count > 0) {
      count -= 1;
      if (DIFF_TWO_CELLS(src, dest)) {
        // dest will be marked dirty, fix the counter
        if (!IS_DIRTY(dest)) cbuf->dirtyCount += 1;
        *dest = *src;
        dest->flags |= CELL_DIRTY;
      }
      src += dir; dest += dir;
    }
  } else {
    // overwrite dirty flags
    while (count > 0) {
      count -= 1;
      if (IS_DIRTY(dest)) {
        // dest is dirty
        if (!IS_DIRTY(src)) cbuf->dirtyCount -= 1;
      } else {
        // dest is not dirty
        if (IS_DIRTY(src)) cbuf->dirtyCount += 1;
      }
      *dest = *src;
      src += dir; dest += dir;
    }
  }
}


//==========================================================================
//
//  cbuf_scroll_area
//
//  vertical area scroll.
//  will scroll the region from `y0` to `y1` (inclusive).
//  if `lines` is negative, scroll up, else scroll down.
//  dirty flags will be copied.
//  it is caller's responsibility to clear new lines.
//
//==========================================================================
YTERM_PUBLIC void cbuf_scroll_area (CellBuffer *cbuf, int y0, int y1, int lines,
                                    int dir, yterm_bool merge_dirty, const CharCell *attr)
{
  if (cbuf == NULL || dir == 0 || lines <= 0) return;

  const int hgt = cbuf->height;

  if (y0 > y1 || y1 < 0 || y0 >= hgt || cbuf->width == 0) return;

  // clip scroll region
  if (y0 < 0) y0 = 0;
  if (y1 >= hgt) y1 = hgt - 1;
  const int regh = y1 - y0 + 1;

  const int wdt = cbuf->width;

  if (lines < regh) {
    const int lcp = (regh - lines) * wdt; // cells to copy

    if (dir < 0) {
      // scroll up
      copy_strip(cbuf, (y0 + lines) * wdt, y0 * wdt, lcp, merge_dirty);
      // clear new lines
      if (attr != NULL) {
        cbuf_clear_region(cbuf, 0, y1 - lines + 1, wdt - 1, y1, attr);
      }
    } else {
      // scroll down
      copy_strip(cbuf, y0 * wdt, (y0 + lines) * wdt, lcp, merge_dirty);
      // clear new lines
      if (attr != NULL) {
        cbuf_clear_region(cbuf, 0, y0, wdt - 1, y0 + lines - 1, attr);
      }
    }

    #ifdef SCROLL_DIRTY_CHECKS
    check_dirty_count(cbuf, __LINE__);
    #endif
  } else {
    // the whole region, just clear it
    if (attr != NULL) {
      cbuf_clear_region(cbuf, 0, y0, wdt - 1, y1, attr);
    }
  }

  // fix autowraps: above the region, and the region itself
  y0 -= 1;
  while (y0 != y1) {
    cbuf_unmark_line_autowrap(cbuf, y0);
    y0 += 1;
  }
}


//==========================================================================
//
//  cbuf_insert_chars
//
//==========================================================================
YTERM_PUBLIC void cbuf_insert_chars (CellBuffer *cbuf, int x, int y, int count,
                                     const CharCell *attr)
{
  if (cbuf == NULL || count <= 0 || y < 0 || y >= cbuf->height) return;

  const int wdt = cbuf->width;

  if (x >= wdt) return;
  if (x < 0) x = 0;

  if (wdt - x > count) {
    // copy
    copy_strip(cbuf, y * wdt + x, y * wdt + x + count, wdt - x - count, CBUF_DIRTY_MERGE);
    // clear
    if (attr != NULL) {
      cbuf_clear_region(cbuf, x, y, x + count - 1, y, attr);
    }
  } else {
    // just clear
    if (attr != NULL) {
      cbuf_clear_region(cbuf, x, y, wdt - 1, y, attr);
    }
  }

  cbuf_unmark_line_autowrap(cbuf, y);

  #ifdef SCROLL_DIRTY_CHECKS
  check_dirty_count(cbuf, __LINE__);
  #endif
}


//==========================================================================
//
//  cbuf_delete_chars
//
//==========================================================================
YTERM_PUBLIC void cbuf_delete_chars (CellBuffer *cbuf, int x, int y, int count,
                                     const CharCell *attr)
{
  if (cbuf == NULL || count <= 0 || y < 0 || y >= cbuf->height) return;

  const int wdt = cbuf->width;

  if (x >= wdt) return;
  if (x < 0) x = 0;

  if (wdt - x > count) {
    // copy
    copy_strip(cbuf, y * wdt + x + count, y * wdt + x, wdt - x - count, CBUF_DIRTY_MERGE);
    // clear
    if (attr != NULL) {
      cbuf_clear_region(cbuf, wdt - count, y, wdt - 1, y, attr);
    }
  } else {
    // just clear
    if (attr != NULL) {
      cbuf_clear_region(cbuf, x, y, wdt - 1, y, attr);
    }
  }

  cbuf_unmark_line_autowrap(cbuf, y);

  #ifdef SCROLL_DIRTY_CHECKS
  check_dirty_count(cbuf, __LINE__);
  #endif
}


//==========================================================================
//
//  cbuf_copy_region
//
//==========================================================================
YTERM_PUBLIC void cbuf_copy_region (CellBuffer *cbuf, int destx, int desty,
                                    const CellBuffer *srcbuf,
                                    int x0, int y0, int x1, int y1)
{
  // fast exit checks
  if (cbuf == NULL || srcbuf == NULL) return;
  if (x1 < 0 || y1 < 0 || x0 > x1 || y0 > y1) return;

  if (destx >= cbuf->width || desty >= cbuf->height) return;
  if (x0 >= srcbuf->width || y0 >= srcbuf->height) return;

  // clip source region
  if (x0 < 0) {
    // shift destination right, to compensate missing part
    destx -= x0; x0 = 0;
    if (destx >= cbuf->width) return;
  }
  if (y0 < 0) {
    // shift destination down, to compensate missing part
    desty -= y0; y0 = 0;
    if (desty >= cbuf->height) return;
  }
  if (x1 >= srcbuf->width) x1 = cbuf->width - 1;
  if (y1 >= srcbuf->height) y1 = cbuf->height - 1;

  // check it again, just in case
  if (x0 > x1 || y0 > y1) return;

  // region size
  int cw = x1 - x0 + 1;
  int ch = y1 - y0 + 1;

  // clip destination region
  if (destx < 0) {
    // shift source right, and shorten
    x0 -= destx; cw += destx; destx = 0;
    if (cw < 1 || x0 >= srcbuf->width) return;
  }
  if (desty < 0) {
    // shift source down, and shorten
    y0 -= desty; ch += desty; desty = 0;
    if (ch < 1 || y0 >= srcbuf->height) return;
  }

  // clip destination even more
  yterm_assert(destx < cbuf->width);
  yterm_assert(desty < cbuf->height);
  cw = min2(cw, cbuf->width - destx);
  ch = min2(ch, cbuf->height - desty);
  yterm_assert(cw > 0);
  yterm_assert(ch > 0);

  // ok, everything is properly clipped now, we can copy cells
  const CharCell *src = &srcbuf->buf[y0 * srcbuf->width + x0];
  CharCell *dest = &cbuf->buf[desty * cbuf->width + destx];

  while (ch != 0) {
    const CharCell *s = src;
    CharCell *d = d;
    int cnt = cw;
    while (cnt != 0) {
      cnt -= 1;
      if (DIFF_TWO_CELLS(s, d)) {
        if (!IS_DIRTY(d)) cbuf->dirtyCount += 1;
        d->ch = s->ch;
        d->flags = s->flags | CELL_DIRTY;
        d->fg = s->fg;
        d->bg = s->bg;
      }
      s += 1; d += 1;
    }
    src += srcbuf->width;
    dest += cbuf->width;
  }

  #ifdef SCROLL_DIRTY_CHECKS
  check_dirty_count(cbuf, __LINE__);
  #endif
}


//==========================================================================
//
//  cbuf_merge_cell
//
//==========================================================================
YTERM_PUBLIC void cbuf_merge_cell (CellBuffer *cbuf, int x, int y, const CharCell *cell) {
  if (cbuf != NULL && cell != NULL && x >= 0 && y >= 0 &&
      x < cbuf->width && y < cbuf->height)
  {
    CharCell *d = &cbuf->buf[y * cbuf->width + x];
    if (DIFF_TWO_CELLS(cell, d)) {
      if (!IS_DIRTY(d)) cbuf->dirtyCount += 1;
      d->ch = cell->ch;
      d->flags = cell->flags | CELL_DIRTY;
      d->fg = cell->fg;
      d->bg = cell->bg;
    }
  }
}
