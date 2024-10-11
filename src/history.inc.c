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
  history file management.
  included directly into the main file.
*/

/*
  history is kept in an anonymous temporary file.
  file format is very easy: it simply keeps the saved CellChars.
  each line is saved in full.
  note that owner must know line width.
*/


#define YT_HISTORY_WRBUF_SIZE  (65536*2)

static CharCell *tmpcbuf = NULL;
static size_t tmpcbuf_size = 0;


//==========================================================================
//
//  history_close
//
//==========================================================================
static void history_close (HistoryFile *file) {
  if (file->fd >= 0) {
    close(file->fd);
    file->fd = -1;
  }
  if (file->wrbuffer != NULL) {
    free(file->wrbuffer);
    file->wrbuffer = NULL;
  }
  file->wrbufferSize = 0;
  file->wrbufferUsed = 0;
  file->wrbufferOfs = 0;
  file->lcount = 0;
}


//==========================================================================
//
//  history_flush
//
//==========================================================================
static yterm_bool history_flush (HistoryFile *file) {
  if (file->fd >= 0) {
    if (file->wrbufferUsed != 0) {
      if (lseek(file->fd, file->wrbufferOfs, SEEK_SET) == (off_t)-1) {
        history_close(file);
        return 0;
      }
      if (write(file->fd, file->wrbuffer, file->wrbufferUsed) != (ssize_t)file->wrbufferUsed) {
        history_close(file);
        return 0;
      }
      file->wrbufferUsed = 0;
      file->wrbufferOfs = 0;
    }
  } else {
    if (file->wrbuffer != NULL) {
      free(file->wrbuffer);
      file->wrbuffer = NULL;
    }
    file->wrbufferSize = 0;
    file->wrbufferUsed = 0;
    file->wrbufferOfs = 0;
    file->lcount = 0;
  }
  return 1;
}


//==========================================================================
//
//  history_reformat
//
//  returns new fd
//
//==========================================================================
static void history_reformat (HistoryFile *file, int newwdt) {
  int newfd = -1;
  CharCell *oline = NULL;
  CharCell *nline = NULL;

  // arbitrary limits
  yterm_assert(newwdt > 0 && newwdt <= 65535);

  (void)history_flush(file);

  if (file->fd == -1 || file->width == newwdt || file->lcount == 0) {
    file->width = newwdt;
    return;
  }

  #if 1
  fprintf(stderr, "HISTORY: reformat %d lines from %d to %d...\n",
          file->lcount, file->width, newwdt);
  #endif

  const size_t oldsz = (unsigned)file->width * sizeof(CharCell);
  const size_t newsz = (unsigned)newwdt * sizeof(CharCell);

  //TODO: relayout it!
  if (lseek(file->fd, 0, SEEK_SET) == (off_t)-1) goto error;

  newfd = create_anon_fd();
  if (newfd == -1) goto error;

  // copy it
  oline = malloc(oldsz);
  if (oline == NULL) goto error;
  nline = malloc(newsz);
  if (nline == NULL) goto error;

  for (int ln = 0; ln < file->lcount; ln += 1) {
    if (read(file->fd, oline, oldsz) != (ssize_t)oldsz) goto error;
    for (int x = 0; x < newwdt; x += 1) {
      if (x < file->width) {
        nline[x] = oline[x];
      } else {
        cbuf_set_cell_defaults(nline + x);
      }
    }
    if (write(newfd, nline, newsz) != (ssize_t)newsz) goto error;
  }

  close(file->fd);
  free(nline); free(oline);
  file->fd = newfd; file->width = newwdt;

  return;

error:
  free(nline); free(oline);
  if (file->fd != -1) close(file->fd);
  if (newfd != -1) close(newfd);
  file->fd = -1; file->width = newwdt; file->lcount = 0;
}


//==========================================================================
//
//  history_opened
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool history_opened (HistoryFile *file) {
  return (file != NULL && file->fd >= 0);
}


//==========================================================================
//
//  history_ensure
//
//==========================================================================
static void history_ensure (HistoryFile *file, int width) {
  if (file != NULL) {
    if (file->fd == -1) {
      file->fd = create_anon_fd();
      file->width = width;
      file->lcount = 0;
      if (file->wrbuffer != NULL) free(file->wrbuffer);
      file->wrbufferSize = 0;
      file->wrbufferUsed = 0;
      file->wrbufferOfs = 0;
    } else if (file->width != width) {
      history_reformat(file, width);
    }
  }
}


//==========================================================================
//
//  history_put_line
//
//  merges with the current line
//
//==========================================================================
static void history_put_line (int cbufy0, int y, HistoryFile *file) {
  if (file == NULL || !history_opened(file)) return;
  if (cbufy0 < 0 || cbufy0 >= hvbuf.height) return;
  if (y < 0 || y >= file->lcount || file->width < 1 || file->width > 65535) return;
  if (history_flush(file) == 0) return;
  const size_t lsz = (unsigned)file->width * sizeof(CharCell);
  if (lseek(file->fd, (off_t)((size_t)lsz * (unsigned)y), SEEK_SET) == (off_t)-1) return;
  if (lsz > tmpcbuf_size) {
    tmpcbuf = realloc(tmpcbuf, lsz);
    yterm_assert(tmpcbuf != NULL); //FIXME: handle this
    tmpcbuf_size = lsz;
  }
  if (read(file->fd, tmpcbuf, lsz) != (ssize_t)lsz) return;
  for (int x = 0; x < file->width; x += 1) {
    cbuf_merge_cell(&hvbuf, x, cbufy0, tmpcbuf + x);
  }
}


//==========================================================================
//
//  history_append_line
//
//==========================================================================
static void history_append_line (CellBuffer *cbuf, int cbufy0, HistoryFile *file) {
  if (cbuf == NULL || file == NULL || cbufy0 < 0 || cbufy0 >= cbuf->height) return;
  // too many lines, wipe the history
  if (file->lcount > 0x00ffffff) {
    #if 1
    fprintf(stderr, "HISTORY: too long, wiping it.\n");
    #endif
    if (file->fd != -1) close(file->fd);
    file->fd = -1;
    file->width = 0;
    file->lcount = 0;
  }
  history_ensure(file, cbuf->width);
  if (!history_opened(file)) return;

  // alloc buffer, if there is none
  if (file->wrbuffer == NULL) {
    file->wrbuffer = malloc(YT_HISTORY_WRBUF_SIZE);
    if (file->wrbuffer != NULL) {
      file->wrbufferSize = YT_HISTORY_WRBUF_SIZE;
      file->wrbufferUsed = 0;
      file->wrbufferOfs = 0;
    }
  }

  const size_t lsz = (unsigned)cbuf->width * sizeof(CharCell);

  if (file->wrbuffer != NULL && file->wrbufferSize - file->wrbufferUsed < lsz &&
      file->wrbufferSize >= lsz)
  {
    if (history_flush(file) == 0) return;
  }

  if (file->wrbuffer == NULL || file->wrbufferSize - file->wrbufferUsed < lsz) {
    if (history_flush(file) == 0) return;
    const off_t npos = (off_t)file->lcount * lsz;
    if (lseek(file->fd, npos, SEEK_SET) == (off_t)-1) goto error;
    if (write(file->fd, &cbuf->buf[cbufy0 * cbuf->width], lsz) != (ssize_t)lsz) goto error;
    file->lcount += 1;
    #if 0
    fprintf(stderr, "HISTORY: added line #%d (bytes: %u)\n", file->lcount, (unsigned)lsz);
    #endif
  } else {
    // append to the buffer
    uint32_t bofs = file->wrbufferUsed;
    if (bofs == 0) file->wrbufferOfs = (off_t)file->lcount * lsz;
    memcpy(file->wrbuffer + bofs, &cbuf->buf[cbufy0 * cbuf->width], lsz);
    bofs += lsz;
    yterm_assert(bofs <= file->wrbufferSize);
    file->wrbufferUsed = bofs;
    #if 0
    fprintf(stderr, "BUFFERED: bofs=%u; afterofs=%u; size=%u\n",
            bofs - lsz, bofs, file->wrbufferSize);
    #endif
    file->lcount += 1;
  }
  return;

error:
  history_close(file);
  file->width = 0;
  return;
}


//==========================================================================
//
//  history_load_line
//
//  load history line into temp buffer
//  coordinate is "global" history coordinate
//
//==========================================================================
static const CharCell *history_load_line (const CellBuffer *cbuf, int gy, HistoryFile *file) {
  if (file == NULL || cbuf == NULL || hvbuf.height == 0 || gy < 0) return NULL;
  const int lcount = (file->fd >= 0 ? file->lcount : 0);
  if (gy >= lcount + hvbuf.height) return NULL;

  if (gy < lcount) {
    // in the history
    if (history_flush(file) == 0) return NULL;
    if (file->fd == -1 || file->width != hvbuf.width) return NULL;
    const size_t lsz = (unsigned)file->width * sizeof(CharCell);
    if (lsz > tmpcbuf_size) {
      tmpcbuf = realloc(tmpcbuf, lsz);
      yterm_assert(tmpcbuf != NULL); //FIXME: handle this
      tmpcbuf_size = lsz;
    }
    if (lseek(file->fd, (off_t)((size_t)lsz * (unsigned)gy), SEEK_SET) != (off_t)-1) {
      if (read(file->fd, tmpcbuf, lsz) == (ssize_t)lsz) {
        return tmpcbuf;
      }
    }
  } else {
    gy -= lcount;
    if (gy >= 0 && gy < cbuf->height) return &cbuf->buf[gy * hvbuf.width];
  }
  return NULL;
}
