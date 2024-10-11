/*
 * YTerm -- (mostly) GNU/Linux X11 terminal emulator
 *
 * Simple cell (character) windows
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
 * this module implements character cell buffer and subwindows in it
 */
#ifndef YTERM_HEADER_CELL_WINDOW
#define YTERM_HEADER_CELL_WINDOW

#include "common.h"


// ////////////////////////////////////////////////////////////////////////// //
// the cell buffer will never be less than this
// it doesn't matter if it cannot fit into the window
// just because
#define MinBufferWidth   (1)
#define MinBufferHeight  (1)
// ...and will never be higher than this
#define MaxBufferWidth   (32767)
#define MaxBufferHeight  (32767)


#define CMODE_NORMAL  (0)
#define CMODE_BW      (1)
#define CMODE_GREEN   (2)
#define CMODE_AMBER   (3)
#define CMODE_AMBER_TINT  (4)

extern int colorMode;


#define CIDX_DEFAULT_FG  (256)
#define CIDX_DEFAULT_BG  (257)

// high-intensity
#define CIDX_DEFAULT_HIGH_FG  (258)
#define CIDX_DEFAULT_HIGH_BG  (259)

// indicies for special colors (foreground only)
#define CIDX_BOLD_FG        (260)
#define CIDX_UNDER_FG       (261)
#define CIDX_BOLD_UNDER_FG  (262)
// with blink set
#define CIDX_BOLD_HIGH_FG        (263)
#define CIDX_UNDER_HIGH_FG       (264)
#define CIDX_BOLD_UNDER_HIGH_FG  (265)

#define CIDX_MAX  (265)

extern uint32_t colorsTTY[CIDX_MAX + 1];
extern uint32_t colorSelectionFG;
extern uint32_t colorSelectionBG;
// amber mode is used when selection mode is active
extern uint32_t colorAmberFG;
extern uint32_t colorAmberBG;

// draw underlines when we cannot use attrs?
extern yterm_bool enableUnderline;

// invert inverse flag ;-)
extern yterm_bool cbufInverse;

// init default palette
void cbuf_init_palette (void);


// ////////////////////////////////////////////////////////////////////////// //
// if `&CELL_ATTR_MASK` is not zero, this is TTY color, not rgb
// TTY color `0xffffU` is "default"
#define CELL_RGB_MASK   (0x00ffffffU)
#define CELL_ATTR_MASK  (0xff000000U)

#define CELL_DEFAULT_ATTR  (0xffffU)

// cells flags
#define CELL_INVERSE    (0x0001U)
#define CELL_UNDERLINE  (0x0002U)
#define CELL_BOLD       (0x0004U)
#define CELL_BLINK      (0x0008U)
// selection
#define CELL_SELECTION  (0x0100U)
// set if this line was automatically wrapped around.
// has any sense only for the last char.
#define CELL_AUTO_WRAP  (0x4000U)
// dirty flag
#define CELL_DIRTY      (0x8000U)


#define CELL_STD_COLOR(c_)  ((uint32_t)(c_)|CELL_ATTR_MASK)


// history buffer will compress lines with the same attrs
typedef struct YTERM_PACKED {
  uint16_t ch; // unicrap char
  uint16_t flags;
  uint32_t fg; // rgb color
  uint32_t bg; // rgb color
} CharCell;


// cell buffer contains two copies of the screen
// the first copy is the one where we're doing all rendering, and
// the second copy is what was rendered last time
// this way no matter how many times the contents of the first copy
// was changed, the backend only have to re-render what was truly changed
// the backend is responsible for syncing copies when rendering, and for
// keeping `dirty_chars` valid
typedef struct {
  int width; // never zero
  int height; // never zero
  CharCell *buf; // current buffer, and temp buffer
  int dirtyCount;
} CellBuffer;


// ////////////////////////////////////////////////////////////////////////// //
// initialize cell buffer, allocate cells
void cbuf_new (CellBuffer *cbuf, int nwidth, int nheight);
// free cell buffer cells, zero out `*cbuf`
void cbuf_free (CellBuffer *cbuf);

// resize cell buffer
void cbuf_resize (CellBuffer *cbuf, int nwidth, int nheight, yterm_bool relayout);


// ////////////////////////////////////////////////////////////////////////// //
YTERM_STATIC_INLINE CharCell *cbuf_cell_at (CellBuffer *cbuf, int x, int y) {
  return (cbuf && x >= 0 && y >= 0 && x < cbuf->width && y < cbuf->height ?
            &cbuf->buf[y * cbuf->width + x] : NULL);
}


// ////////////////////////////////////////////////////////////////////////// //
// all output pointers should be non-NULL
void cbuf_decode_attrs (const CharCell *cc, uint32_t *fg, uint32_t *bg, yterm_bool *under);

void cbuf_set_cell_defaults (CharCell *cell);


// ////////////////////////////////////////////////////////////////////////// //
// region is inclusive
void cbuf_mark_region_dirty (CellBuffer *cbuf, int x0, int y0, int x1, int y1);
void cbuf_mark_region_clean (CellBuffer *cbuf, int x0, int y0, int x1, int y1);

void cbuf_mark_line_autowrap (CellBuffer *cbuf, int y);
void cbuf_unmark_line_autowrap (CellBuffer *cbuf, int y);

// this is called on screen switching.
// only non-dirty and totally unchanged cells will remain non-dirty.
void cbuf_merge_all_dirty (CellBuffer *cbuf, const CellBuffer *sbuf);

YTERM_STATIC_INLINE void cbuf_mark_dirty (CellBuffer *cbuf, int x, int y) {
  cbuf_mark_region_dirty(cbuf, x, y, x, y);
}

YTERM_STATIC_INLINE void cbuf_mark_line_dirty (CellBuffer *cbuf, int y) {
  if (cbuf != NULL) cbuf_mark_region_dirty(cbuf, 0, y, cbuf->width - 1, y);
}

YTERM_STATIC_INLINE void cbuf_mark_all_dirty (CellBuffer *cbuf) {
  if (cbuf != NULL) cbuf_mark_region_dirty(cbuf, 0, 0, cbuf->width - 1, cbuf->height - 1);
}

YTERM_STATIC_INLINE void cbuf_mark_all_clean (CellBuffer *cbuf) {
  if (cbuf != NULL) cbuf_mark_region_clean(cbuf, 0, 0, cbuf->width - 1, cbuf->height - 1);
}


// ////////////////////////////////////////////////////////////////////////// //
// if `attr` is `NULL`, use default colors.
void cbuf_write_wchar_count (CellBuffer *cbuf, int x, int y, uint32_t ch,
                             int count, const CharCell *attr);

YTERM_STATIC_INLINE void cbuf_write_wchar (CellBuffer *cbuf, int x, int y, uint32_t ch,
                                           const CharCell *attr)
{
  cbuf_write_wchar_count(cbuf, x, y, ch, 1, attr);
}


// ////////////////////////////////////////////////////////////////////////// //
// if `attr` is `NULL`, use default colors.
void cbuf_write_str_utf (CellBuffer *cbuf, int x, int y, const char *str, const CharCell *attr);

enum {
  CBUF_DIRTY_OVERWRITE = 0,
  CBUF_DIRTY_MERGE = 1,
};

// vertical area scroll.
// will scroll the region from `y0` to `y1` (inclusive).
// dirty flags will be copied.
// it is caller's responsibility to clear new lines.
// `merge_dirty` tells if dirty state should be merged, or overwritten.
// `dir` is `-1` or `1` (actually, positive or negative).
// if `attr` is `NULL`, do not clear scrolled out region.
// `attr->ch` will be used as fill char.
void cbuf_scroll_area (CellBuffer *cbuf, int y0, int y1, int lines,
                       int dir, yterm_bool merge_dirty, const CharCell *attr);

YTERM_STATIC_INLINE void cbuf_scroll_area_up (CellBuffer *cbuf, int y0, int y1, int lines,
                                              yterm_bool merge_dirty, const CharCell *attr)
{
  cbuf_scroll_area(cbuf, y0, y1, lines, -1, merge_dirty, attr);
}

YTERM_STATIC_INLINE void cbuf_scroll_area_down (CellBuffer *cbuf, int y0, int y1, int lines,
                                                yterm_bool merge_dirty, const CharCell *attr)
{
  cbuf_scroll_area(cbuf, y0, y1, lines, 1, merge_dirty, attr);
}

// region is inclusive.
// if `attr` is `NULL`, use default colors and space.
// `attr->ch` will be used as fill char.
void cbuf_clear_region (CellBuffer *cbuf, int x0, int y0, int x1, int y1,
                        const CharCell *attr);

// insert blank chars into line.
// if `attr` is `NULL`, use default colors and space.
// `attr->ch` will be used as fill char.
void cbuf_insert_chars (CellBuffer *cbuf, int x, int y, int count, const CharCell *attr);

// delete blank chars from line.
// if `attr` is `NULL`, use default colors and space.
// `attr->ch` will be used as fill char.
void cbuf_delete_chars (CellBuffer *cbuf, int x, int y, int count, const CharCell *attr);


// ////////////////////////////////////////////////////////////////////////// //
// region is inclusive.
// copy region from `srcbuf` to `cbuf`
// perform proper cliping; out-ouf-bounds cells will not be copied.
// this correctly sets dirty status of `cbuf` (source dirty status is ignored).
void cbuf_copy_region (CellBuffer *cbuf, int destx, int desty,
                       const CellBuffer *srcbuf, int x0, int y0, int x1, int y1);


// ////////////////////////////////////////////////////////////////////////// //
void cbuf_merge_cell (CellBuffer *cbuf, int x, int y, const CharCell *cell);


#endif
