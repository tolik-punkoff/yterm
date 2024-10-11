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
  included directly into "term_lib.inc.c".
*/


//==========================================================================
//
//  dbg_csi_dump
//
//==========================================================================
static __attribute__((unused)) void dbg_csi_dump (Term *term) {
  fprintf(stderr, "^[");
  if (term->escseq.priv > 0) fputc('?', stderr);
  else if (term->escseq.priv < 0) fputc('>', stderr);
  for (int f = 0; f < term->escseq.argc; f += 1) {
    if (f != 0) fputc(';', stderr);
    if (f < YTERM_ESC_ARG_SIZ) {
      if (term->escseq.argv[f] != -1) {
        fprintf(stderr, "%d", term->escseq.argv[f]);
      }
    }
  }
  if (term->escseq.cmd) {
    uint32_t c = (unsigned char)term->escseq.cmd;
    if (c > 0x20 && c < 0x7f) fputc(c, stderr);
    else if (c == '\n') fprintf(stderr, "(\\n)");
    else if (c == '\r') fprintf(stderr, "(\\r)");
    else if (c == 0x1b) fprintf(stderr, "(\\e)");
    else fprintf(stderr, "(%u)", c);
  } else {
    fprintf(stderr, "<nocmd>");
  }
  fputc('\n', stderr);
}


//==========================================================================
//
//  csi_bad
//
//==========================================================================
static void csi_bad (Term *term, int erridx) {
  #if 1
  if (erridx >= 0) {
    fprintf(stderr, "ERROR: bad CSI!\n  ");
  } else {
    fprintf(stderr, "ERROR: bad CSI (at arg #%d)!\n  ", erridx + 1);
  }
  dbg_csi_dump(term);
  #endif
}


// don't convert 0 to 1
#define CSI_DEFARG0(idx_)  \
  ((idx_) >= 0 && (idx_) < YTERM_ESC_ARG_SIZ && term->escseq.argv[(idx_)] != -1 \
    ? term->escseq.argv[(idx_)] : 0)

// convert 0 to 1
// it looks like most commands always clamps at 1, oops
// dunno if it is right, but it makes "vtest" happy
#define CSI_DEFARG1(idx_)  \
  ((idx_) >= 0 && (idx_) < YTERM_ESC_ARG_SIZ && term->escseq.argv[(idx_)] > 0 \
    ? term->escseq.argv[(idx_)] : 1)

// convert 0 to 1
// it looks like most commands always clamps at 1, oops
// dunno if it is right, but it makes "vtest" happy
#define CSI_DEFARG1N(idx_,defv_)  \
  ((idx_) >= 0 && (idx_) < YTERM_ESC_ARG_SIZ && term->escseq.argv[(idx_)] > 0 \
    ? term->escseq.argv[(idx_)] : (defv_))


//==========================================================================
//
//  csi_set_attr
//
//==========================================================================
static void csi_set_attr (Term *term, int *attr, int argc) {
  if (!term) return;
  int tmpattr[1];
  if (!attr || argc == 0) {
    tmpattr[0] = 0;
    attr = tmpattr;
    argc = 1;
  }
  #if 0
  fprintf(stderr, "SGR: argc=%d\n", argc);
  for (int f = 0; f < argc; f += 1) fprintf(stderr, "  #%d: %d\n", f, attr[f]);
  #endif
  int aidx = 0;
  do {
    int aa = (aidx < argc ? attr[aidx] : 0);
    if (aa < 0) aa = 0;
    aidx += 1;
    switch (aa) {
      case 0:
        TERM_CATTR(term)->flags = 0;
        TERM_CATTR(term)->fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
        TERM_CATTR(term)->bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
        break;
      case 1:
        TERM_CATTR(term)->flags |= CELL_BOLD;
        break;
      case 4:
        TERM_CATTR(term)->flags |= CELL_UNDERLINE;
        break;
      case 5:
        TERM_CATTR(term)->flags |= CELL_BLINK;
        break;
      case 7:
        TERM_CATTR(term)->flags |= CELL_INVERSE;
        break;
      case 22:
        TERM_CATTR(term)->flags &= ~CELL_BOLD;
        break;
      case 24:
        TERM_CATTR(term)->flags &= ~CELL_UNDERLINE;
        break;
      case 25:
        TERM_CATTR(term)->flags &= ~CELL_BLINK;
        break;
      case 27:
        TERM_CATTR(term)->flags &= ~CELL_INVERSE;
        break;
      case 38: // foreground
        if (aidx + 1 < argc && attr[aidx] == 5) {
          // xterm 256 color attr: "38;5;num"
          aa = max2(0, attr[aidx + 1]);
          aidx += 2;
          if (aa >= 0) {
            TERM_CATTR(term)->fg = CELL_STD_COLOR(aa % 256);
          } else {
            //fprintf(stderr, "erresc: bad fgcolor %d\n", aa);
            TERM_CATTR(term)->fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
          }
        } else if (aidx < argc && attr[aidx] == 2) {
          // 38;2;r;g;b
          // xterm says: 38;2;palidx;r;g;b
          aidx += 1;
          if (aidx + 2 < argc && attr[aidx] >= 0 && attr[aidx + 1] >= 0 && attr[aidx + 2] >= 0) {
            const uint32_t r = (uint32_t)(attr[aidx + 0] & 0xff);
            const uint32_t g = (uint32_t)(attr[aidx + 1] & 0xff);
            const uint32_t b = (uint32_t)(attr[aidx + 2] & 0xff);
            TERM_CATTR(term)->fg = (r<<16)|(g<<8)|b;
          }
          aidx += 3;
        } else {
          TERM_CATTR(term)->fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
          //fprintf(stderr, "erresc: gfx attr %d unknown\n", aa);
        }
        break;
      case 39:
        TERM_CATTR(term)->fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
        break;
      case 48: // background
        if (aidx + 1 < argc && attr[aidx] == 5) {
          // xterm 256 color attr: "38;5;num"
          aa = max2(0, attr[aidx + 1]);
          aidx += 2;
          if (aa >= 0) {
            TERM_CATTR(term)->bg = CELL_STD_COLOR(aa % 256);
          } else {
            //fprintf(stderr, "erresc: bad bgcolor %d\n", aa);
            TERM_CATTR(term)->bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
          }
        } else if (aidx < argc && attr[aidx] == 2) {
          // 48;2;r;g;b
          aidx += 1;
          if (aidx + 2 < argc && attr[aidx] >= 0 && attr[aidx + 1] >= 0 && attr[aidx + 2] >= 0) {
            const uint32_t r = (uint32_t)(attr[aidx + 0] & 0xff);
            const uint32_t g = (uint32_t)(attr[aidx + 1] & 0xff);
            const uint32_t b = (uint32_t)(attr[aidx + 2] & 0xff);
            TERM_CATTR(term)->bg = (r<<16)|(g<<8)|b;
          }
          aidx += 3;
        } else {
          //fprintf(stderr, "erresc: gfx attr %d unknown\n", aa);
          TERM_CATTR(term)->bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
        }
        break;
      case 49:
        TERM_CATTR(term)->bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
        break;
      default:
        if (aa >= 30 && aa <= 37) {
          TERM_CATTR(term)->fg = CELL_STD_COLOR(aa - 30);
        } else if (aa >= 40 && aa <= 47) {
          TERM_CATTR(term)->bg = CELL_STD_COLOR(aa - 40);
        } else if (aa >= 90 && aa <= 97) {
          TERM_CATTR(term)->fg = CELL_STD_COLOR(aa - 90 + 8);
        } else if (aa >= 100 && aa <= 107) {
          TERM_CATTR(term)->bg = CELL_STD_COLOR(aa - 100 + 8);
        } else {
          //fprintf(stderr, "erresc: gfx attr %d unknown\n", aa); k8t_dbgCSIDump(term);
        }
        break;
    }
  } while (aidx < argc);
}


//==========================================================================
//
//  csi_next_char
//
//  return non-zero if the sequence is finished
//  control char (and 0x7f) will never arrive here
//
//  this is basically a state machine with a fucked up state detection
//  (the state is spread across several vars).
//  it is done this way because i am an idiot.
//
//==========================================================================
static yterm_bool csi_next_char (Term *term, uint32_t cp) {
  yterm_bool done = 0;

  /*
    this parser is not completely right, i believe, because ';;m'
    will register two args, not three. i'm not sure that this is
    right, but meh.
  */

  // check for private sequence
  // this allows several private sequence marks, but meh
  if (cp == '?' && term->escseq.argc == 0) {
    term->escseq.priv = 1;
  } else if (cp == '>' && term->escseq.argc == 0) {
    term->escseq.priv = -1;
  } else if (cp == ';' || cp == ':') {
    // finish current arg, advance to the next one
    // yeah, ";" starts a new arg, always
    if (term->escseq.argc == 0) {
      // ";" before any arg means that we omited the first arg
      term->escseq.argv[0] = -1;
      term->escseq.argc = 1;
    }
    // mark new arg as "default" (it is not registered yet)
    if (term->escseq.argc < YTERM_ESC_ARG_SIZ) {
      term->escseq.argv[term->escseq.argc] = -1;
    }
    term->escseq.argc = min2(YTERM_ESC_ARG_SIZ, term->escseq.argc + 1);
  /*
  } else if (cp == ':') {
    // keep parsing, but ignore
    term->escseq.state = YTERM_ESC_CSI_IGNORE;
  */
  } else if (cp >= '0' && cp <= '9') {
    // digit
    int idx = term->escseq.argc - 1;
    if (idx == -1) {
      // no args yet, this is the first one
      term->escseq.argc = 1;
      idx = 0;
      term->escseq.argv[0] = 0;
    }
    if (idx < YTERM_ESC_ARG_SIZ) {
      int val = term->escseq.argv[idx];
      if (val == -1) val = 0;
      // arbitrary clamping
      val = (val * 10 + (cp - '0')) & 0xffff;
      term->escseq.argv[idx] = val;
    }
  } else if (cp >= 0x40 && cp <= 0x7E) {
    // final char -- command
    term->escseq.cmd = (char)cp;
    done = 1;
  } else {
    /* ok, specs says that other crap is a part of a command.
       don't bother executing it, simply ignore.
    // invalid char, abort
    done = 1;
    reset_esc(term);
    */
    term->escseq.state = YTERM_ESC_CSI_IGNORE;
  }

  return done;
}


//==========================================================================
//
//  csi_priv_xt
//
//  extended CSI sequence (">")
//
//==========================================================================
static void csi_priv_xt (Term *term) {
  switch (term->escseq.cmd) {
    case 'T': // reset title mode; not implemented, and ingored
      // see https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
      /*
        Ps = 0  --  Do not set window/icon labels using hexadecimal.
        Ps = 1  --  Do not query window/icon labels using hexadecimal.
        Ps = 2  --  Do not set window/icon labels using UTF-8.
        Ps = 3  --  Do not query window/icon labels using UTF-8.
      */
      break;
    case 'c': // send/query device attrs
      if (term->escseq.argc == 0 || (term->escseq.argc == 1 && term->escseq.argv[0] <= 0)) {
        // Pp = 0 -- "VT100".
        // Pp = 1 -- "VT220".
        // Pp = 2 -- "VT240" or "VT241".
        // then firmware version, then 0
        term_write_raw(term, "\x1b[>0;69;0c", -1);
      }
      break;
  }
}


//==========================================================================
//
//  term_ensure_alt_buffer
//
//==========================================================================
static void term_ensure_alt_buffer (Term *term) {
  if (term != NULL && term->alt.cbuf.width == 0) {
    term->alt.curpos.x = 0; term->alt.curpos.y = 0;
    term->alt.curpos.lastColFlag = 0;
    term->alt.curpos.justMoved = 0;
    term->alt.cursaved = term->alt.curpos;
    term->alt.curhidden = 0;
    term->alt.currattr.ch = 0x20;
    term->alt.currattr.flags = 0;
    term->alt.currattr.fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
    term->alt.currattr.bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
    term->alt.scTop = 0; term->alt.scBot = term->main.cbuf.height - 1;
    term->alt.attrsaved = term->alt.currattr;
    term->alt.charsetsaved = 0;
    term->alt.modesaved = 0;
    cbuf_new(&term->alt.cbuf, term->main.cbuf.width, term->main.cbuf.height);
  }
}


//==========================================================================
//
//  term_release_alt_buffer
//
//  free alt buffer if possible
//
//==========================================================================
static void term_release_alt_buffer (Term *term) {
  if (term != NULL && term->alt.cbuf.width != 0 && term->wkscr == &term->main) {
    cbuf_free(&term->alt.cbuf);
  }
}


//==========================================================================
//
//  term_switch_screen
//
//==========================================================================
static void term_switch_screen (Term *term) {
  term_invalidate_cursor(term);
  if (term->scroll_count != 0) {
    term->scroll_count = 0;
    cbuf_mark_all_clean(&term->wkscr->cbuf);
    if (term->wkscr == &term->main) {
      // main -> alt
      term->wkscr = &term->alt;
      // if we don't have alt buffer yet, create it
      term_ensure_alt_buffer(term);
    } else {
      // alt -> main
      term->wkscr = &term->main;
    }
    cbuf_mark_all_dirty(&term->wkscr->cbuf);
  } else if (term->wkscr == &term->main) {
    // main -> alt
    // if we don't have alt buffer yet, create it
    term_ensure_alt_buffer(term);
    cbuf_merge_all_dirty(&term->alt.cbuf, &term->main.cbuf);
    cbuf_mark_all_clean(&term->main.cbuf);
    term->wkscr = &term->alt;
  } else {
    // alt -> main
    cbuf_merge_all_dirty(&term->main.cbuf, &term->alt.cbuf);
    cbuf_mark_all_clean(&term->alt.cbuf);
    term->wkscr = &term->main;
  }
  term_invalidate_cursor(term);
}


//==========================================================================
//
//  csi_priv_yterm
//
//==========================================================================
static yterm_bool csi_priv_yterm (Term *term, yterm_bool mode_h) {
  if (term->escseq.argc > 0 && term->escseq.argv[0] == 666) {
    /* yterm private: 666;69;669;code */
    if (term->escseq.argc < 4) return 0;
    if (term->escseq.argv[1] != 69) return 0;
    if (term->escseq.argv[2] != 669) return 0;
    for (int f = 3; f < term->escseq.argc; f += 1) {
      switch (term->escseq.argv[f]) {
        case 1: // normal color
          term->colorMode = CMODE_NORMAL;
          cbuf_mark_all_dirty(TERM_CBUF(term));
          break;
        case 2: // b/w color
          term->colorMode = CMODE_BW;
          cbuf_mark_all_dirty(TERM_CBUF(term));
          break;
        case 3: // green color
          term->colorMode = CMODE_GREEN;
          cbuf_mark_all_dirty(TERM_CBUF(term));
          break;
        case 13: // turn modificator keys reporting on/off
          term->reportMods = mode_h;
          break;
        case 27: // turn esc-esc mode on/off
          term->escesc = mode_h;
          break;
        case 142: // save/restore modes
          if (mode_h) {
            // h: save
            term->savedValues = 0;
            term->savedValues |= 0x01 | (term->reportMods ? 0x02 : 0);
            term->savedValues |= 0x04;
            if (term->escesc < 0) term->savedValues |= 0x08;
            else if (term->escesc > 0) term->savedValues |= 0x10;
            #if 0
            fprintf(stderr, "SAVE: sv: 0x%04x; rm:%d; escesc:%d\n",
                    term->savedValues, term->reportMods, term->escesc);
            #endif
          } else {
            // l: restore
            if ((term->savedValues & 0x01) != 0) {
              term->reportMods = (term->savedValues & 0x02 ? 1 : 0);
            }
            if ((term->savedValues & 0x04) != 0) {
              if (term->savedValues & 0x08) term->escesc = -1;
              else if (term->savedValues & 0x10) term->escesc = 1;
              else term->escesc = 0;
            }
            #if 0
            fprintf(stderr, "RESTORE: sv: 0x%04x; rm:%d; escesc:%d\n",
                    term->savedValues, term->reportMods, term->escesc);
            #endif
            term->savedValues = 0;
          }
          break;
        case 69: // turn history on/off
          if (mode_h) {
            term->history.enabled = 1;
          } else {
            history_close(&term->history.file);
            term->history.file.width = TERM_CBUF(term)->width;
            if (term->history.blocktype != SBLOCK_NONE) {
              term->history.blocktype = SBLOCK_NONE;
              cbuf_mark_all_dirty(TERM_CBUF(term));
            }
          }
          break;
        default:
          fprintf(stderr, "INVALID YTERM PRIVATE OSC: %d\n", term->escseq.argv[f]);
          break;
      }
    }
    return 1;
  }
  return 0;
}


//==========================================================================
//
//  csi_priv_h
//
//==========================================================================
static void csi_priv_h (Term *term) {
  if (csi_priv_yterm(term, 1)) return;
  for (int aidx = 0; aidx < term->escseq.argc; aidx += 1) {
    switch (term->escseq.argv[aidx]) {
      case 1: /* Application Cursor Keys (DECCKM), VT100 */
        term->mode |= YTERM_MODE_APPKEYPAD;
        break;
      case 2: /* DECANM -- reset all charsets */
        term->mode &= ~(YTERM_MODE_GFX0 | YTERM_MODE_GFX1);
        break;
      case 3: /* DECCOLM -- 132 column mode */
        term_lcf_reset(term);
        if (opt_vttest) {
          const int wwold = winWidth;
          winWidth = charWidth * 132;
          term_fix_size(term);
          winWidth = wwold;
        }
        // just clear the screen
        cbuf_clear_region(TERM_CBUF(term), 0, 0,
                          TERM_CBUF(term)->width - 1, TERM_CBUF(term)->height - 1,
                          TERM_CATTR(term));
        term_curmove_abs(term, 0, 0);
        break;
      case 4: /* DECSCLM -- smooth scroll mode */
        break;
      case 5: /* DECSCNM -- Reverve video */
        if ((term->mode & YTERM_MODE_REVERSE) == 0) {
          term->mode |= YTERM_MODE_REVERSE;
          cbuf_mark_all_dirty(TERM_CBUF(term));
        }
        break;
      case 6: /* DECOM -- origin mode: vertical cursor pos is relative to scroll region */
        term_lcf_reset_nocmove(term);
        term->mode |= YTERM_MODE_ORIGIN;
        // i don't know if this is required; xterm seems to do it
        term_curmove_abs(term, 0, 0);
        break;
      case 7: /* DECAWM -- autowrap on */
        if ((term->mode & YTERM_MODE_WRAP) == 0) {
          // setting autowrap will not reset the LCF
          term->mode |= YTERM_MODE_WRAP;
        }
        break;
      case 8: /* DECARM -- autorepeat keys */
        break;
      case 20: /* non-standard code? */
        term->mode |= YTERM_MODE_CRLF;
        break;
      case 12: /* att610 -- Start blinking cursor (IGNORED) */
        break;
      case 25: /* show cursor */
        if (TERM_CURHIDDEN(term)) {
          term->wkscr->curhidden = 0;
          term_invalidate_cursor(term);
        }
        break;
      case 40: /* xterm: allow 80 -> 132 mode: silently ignore */
        break;
      case 1000: /* 1000,1002: enable xterm mouse report */
        term->mode |= YTERM_MODE_MOUSEBTN;
        break;
      case 1002:
        term->mode |= YTERM_MODE_MOUSEMOTION;
        break;
      case 1004:
        term->mode |= YTERM_MODE_FOCUSEVT;
        break;
      case 1005: /* utf-8 mouse encoding */
      case 1006: /* sgr mouse encoding */
      case 1015: /* urxvt mouse encoding */
        term->mreport.mode = term->escseq.argv[aidx];
        break;
      case 1046: /* enable altscreen */
        #if 1
        if ((term->mode & YTERM_MODE_NO_ALTSCR) != 0) {
          fprintf(stderr, "WARNING: enabled altscreen!\n");
        }
        #endif
        term->mode &= ~YTERM_MODE_NO_ALTSCR;
        break;
      case 1049: /* = 1047 and 1048 */
      case 47:
      case 1047:
        // switch to alt
        if (term->wkscr == &term->main && (term->mode & YTERM_MODE_NO_ALTSCR) == 0) {
          // 1049 saves the cursor before the switch
          if (term->escseq.argv[aidx] == 1049) term_save_cursor(term, SaveXTerm);
          term_switch_screen(term);
          // 1049 should clear the buffer; assume default settings too
          if (term->escseq.argv[aidx] == 1049) {
            term->wkscr->currattr.flags = 0;
            term->wkscr->currattr.fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
            term->wkscr->currattr.bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
            cbuf_clear_region(TERM_CBUF(term), 0, 0,
                              TERM_CBUF(term)->width - 1, TERM_CBUF(term)->height - 1,
                              TERM_CATTR(term));
          }
        }
        break;
      case 1036: /* xterm: send ESC with meta */
      case 1039: /* xterm: send ESC with meta */
        break;
      case 1048: /* save cursor position */
        term_save_cursor(term, SaveXTerm);
        break;
      case 1061: /* set VT220 keyboard emulation */
        /* Home -> ^[[1~
           End  -> ^[[4~
         */
         term->mode |= YTERM_MODE_VT220_KBD;
         term->mode |= YTERM_MODE_XTERM;
         break;
      case 2004: /* set bracketed paste mode */
        term->mode |= YTERM_MODE_BRACPASTE;
        break;
      default:
        csi_bad(term, aidx);
        break;
    }
  }
}


//==========================================================================
//
//  csi_priv_l
//
//==========================================================================
static void csi_priv_l (Term *term) {
  if (csi_priv_yterm(term, 0)) return;
  for (int aidx = 0; aidx < term->escseq.argc; aidx += 1) {
    switch (term->escseq.argv[aidx]) {
      case 1: /* Application Cursor Keys (DECCKM), VT100 */
        term->mode &= ~YTERM_MODE_APPKEYPAD;
        break;
      case 3: /* DECCOLM -- 80 column mode */
        term_lcf_reset(term);
        if (opt_vttest) {
          const int wwold = winWidth;
          winWidth = charWidth * 80;
          term_fix_size(term);
          winWidth = wwold;
        }
        // just clear the screen
        cbuf_clear_region(TERM_CBUF(term), 0, 0,
                          TERM_CBUF(term)->width - 1, TERM_CBUF(term)->height - 1,
                          TERM_CATTR(term));
        term_curmove_abs(term, 0, 0);
        break;
      case 4: /* DECSCLM -- fast scroll mode */
        break;
      case 5: /* DECSCNM -- Remove reverse video */
        if ((term->mode & YTERM_MODE_REVERSE) != 0) {
          term->mode &= ~YTERM_MODE_REVERSE;
          cbuf_mark_all_dirty(TERM_CBUF(term));
        }
        break;
      case 6: /* DECOM -- normal cursor mode */
        term_lcf_reset_nocmove(term);
        term->mode &= ~YTERM_MODE_ORIGIN;
        // i don't know if this is required; xterm seems to do it
        term_curmove_abs(term, 0, 0);
        break;
      case 8: /* DECARM -- no autorepeat keys: wtf?! */
        break;
      case 7: /* autowrap off */
        term_lcf_reset_nocmove(term);
        term->mode &= ~YTERM_MODE_WRAP;
        break;
      case 12: /* att610 -- Stop blinking cursor (IGNORED) */
        break;
      case 20: /* non-standard code? */
        term->mode &= ~YTERM_MODE_CRLF;
        break;
      case 25: /* hide cursor */
        if (TERM_CURVISIBLE(term)) {
          term_invalidate_cursor(term);
          term->wkscr->curhidden = 1;
        }
        break;
      case 45: /* xterm: no reverse wraparound mode */
        break;
      case 1000: /* disable X11 xterm mouse reporting */
        term->mode &= ~YTERM_MODE_MOUSEBTN;
        if ((term->mode & YTERM_MODE_MOUSE) == 0) {
          term->mreport.lastx = -1;
          term->mreport.lasty = -1;
        }
        break;
      case 1001: /* xterm: don't use highlight mouse tracking */
        break;
      case 1002:
        term->mode &= ~YTERM_MODE_MOUSEMOTION;
        if ((term->mode & YTERM_MODE_MOUSE) == 0) {
          term->mreport.lastx = -1;
          term->mreport.lasty = -1;
        }
        break;
      case 1003: /* xterm: don't use motion mouse tracking */
        break;
      case 1004:
        term->mode &= ~YTERM_MODE_FOCUSEVT;
        break;
      case 1005: /* utf-8 mouse encoding */
      case 1006: /* sgr mouse encoding */
      case 1015: /* urxvt mouse encoding */
        term->mreport.mode = 1000;
        break;
      case 1046: /* disable altscreen */
        #if 1
        if ((term->mode & YTERM_MODE_NO_ALTSCR) == 0) {
          fprintf(stderr, "WARNING: disabled altscreen!\n");
        }
        #endif
        term->mode |= YTERM_MODE_NO_ALTSCR;
        if (term->wkscr == &term->alt) term_switch_screen(term);
        break;
      case 1049: /* = 1047 and 1048 */
      case 47:
      case 1047:
        // switch to main
        if (term->wkscr == &term->alt && (term->mode & YTERM_MODE_NO_ALTSCR) == 0) {
          // 10xx clears the alternate screen (at least it seems so accroding to xterm docs)
          if (term->escseq.argv[aidx] != 47) {
            term->wkscr->currattr.flags = 0;
            term->wkscr->currattr.fg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
            term->wkscr->currattr.bg = CELL_STD_COLOR(CELL_DEFAULT_ATTR);
            cbuf_clear_region(TERM_CBUF(term), 0, 0,
                              TERM_CBUF(term)->width - 1, TERM_CBUF(term)->height - 1,
                              TERM_CATTR(term));
          }
          term_switch_screen(term);
          // 1049 restores the cursor after the switch
          if (term->escseq.argv[aidx] == 1049) term_restore_cursor(term, SaveXTerm);
        }
        break;
      case 1034: /* xterm: don't interpret meta key */
        break;
      case 1048: /* restore saved cursor position */
        term_restore_cursor(term, SaveXTerm);
        break;
      case 1061: /* reset VT220 keyboard emulation */
        /* Home -> ^[[H
           End  -> ^[[F
         */
         term->mode &= ~YTERM_MODE_VT220_KBD;
         term->mode |= YTERM_MODE_XTERM;
         break;
      case 2004: /* reset bracketed paste mode */
        term->mode &= ~YTERM_MODE_BRACPASTE;
        break;
      default:
        csi_bad(term, aidx);
        break;
    }
  }
}


//==========================================================================
//
//  csi_priv
//
//  private CSI sequence ("?")
//
//==========================================================================
static void csi_priv (Term *term) {
  char buf[32];
  switch (term->escseq.cmd) {
    case 'l': /* RM -- Reset Mode */
      csi_priv_l(term);
      break;
    case 'h': /* SM -- Set terminal mode */
      csi_priv_h(term);
      break;
    case 'n':
      switch (CSI_DEFARG0(0)) {
        case 6: /* cursor position report */
          sprintf(buf, "\x1b[?%d;%dR", TERM_CPOS(term)->y + 1, TERM_CPOS(term)->x + 1);
          term_write_raw(term, buf, -1);
          break;
      }
      break;
    case 'r': /* xterm: restore private CSI options (list) */
      break;
    case 's': /* xterm: save private CSI options (list) */
      break;
    default:
      csi_bad(term, -1);
      break;
  }
}


//==========================================================================
//
//  csi_normal
//
//==========================================================================
static void csi_normal (Term *term) {
  char buf[32];
  int aidx;
  CursorPos *cpos = TERM_CPOS(term);
  CellBuffer *cbuf = TERM_CBUF(term);
  const CharCell *defattr = TERM_CATTR(term);
  const int wdt = cbuf->width;
  const int hgt = cbuf->height;
  const int stop = TERM_SCTOP(term);
  const int sbot = TERM_SCBOT(term);
  // it looks like most commands always clamps at 1, oops
  // dunno if it is right, but it makes "vtest" happy
  int arg0 = CSI_DEFARG1(0);
  switch (term->escseq.cmd) {
    case '@': /* ICH -- Insert <n> blank chars */
      term_lcf_reset(term);
      cbuf_insert_chars(cbuf, cpos->x, cpos->y, arg0, defattr);
      break;
    case 'A': /* CUU -- Cursor <n> Up */
      term_curmove_rel(term, 0, -arg0);
      break;
    case 'B': /* CUD -- Cursor <n> Down */
    case 'e':
      term_curmove_rel(term, 0, arg0);
      break;
    case 'C': /* CUF -- Cursor <n> Forward */
    case 'a':
      term_curmove_rel(term, arg0, 0);
      break;
    case 'D': /* CUB -- Cursor <n> Backward */
      term_curmove_rel(term, -arg0, 0);
      break;
    case 'E': /* CNL -- Cursor <n> Down and first col */
      term_curmove_rel(term, -cpos->x, arg0);
      break;
    case 'F': /* CPL -- Cursor <n> Up and first col */
      term_curmove_rel(term, -cpos->x, -arg0);
      break;
    case 'G': /* CHA -- Move to <col> */
    case '`': /* XXX: HPA -- same? */
      term_curmove_abs(term, arg0 - 1, cpos->y);
      break;
    case 'd': /* VPA: Move to <row> */
      term_curmove_abs(term, cpos->x, arg0 - 1);
      break;
    case 'H': /* CUP -- Move to <row> <col> */
    case 'f': /* XXX: HVP -- same? */
      term_curmove_abs(term, CSI_DEFARG1(1) - 1, arg0 - 1);
      break;
    case 'I': /* CHT -- Cursor Forward Tabulation <n> tab stops */
      term_lcf_reset(term);
      //FIXME: i can avoid the loop here, but meh
      {
        int nx = cpos->x;
        while (arg0 != 0 && nx != wdt - 1) {
          arg0 -= 1;
          nx = min2((nx | 7) + 1, wdt - 1);
        }
        if (nx != cpos->x) {
          term_invalidate_cursor(term);
          cpos->x = nx;
          term_invalidate_cursor(term);
          cpos->justMoved = 1;
        }
      }
      break;
    case 'J': /* ED -- Clear screen */
      term_lcf_reset(term);
      switch (CSI_DEFARG0(0)) {
        case 0: /* from cursor, and below */
          // current line
          cbuf_clear_region(cbuf, cpos->x, cpos->y, wdt - 1, cpos->y, defattr);
          // other lines
          cbuf_clear_region(cbuf, 0, cpos->y + 1, wdt - 1, hgt - 1, defattr);
          break;
        case 1: /* from top to cursor; inclusive */
          // current line
          cbuf_clear_region(cbuf, 0, cpos->y, cpos->x, cpos->y, defattr);
          // other lines
          cbuf_clear_region(cbuf, 0, 0, wdt - 1, cpos->y - 1, defattr);
          break;
        case 2: /* all */
        case 3: /* all, and scrollback; no way */
          cbuf_clear_region(cbuf, 0, 0, wdt - 1, hgt - 1, defattr);
          break;
        default:
          csi_bad(term, -1);
          break;
      }
      break;
    case 'K': /* EL -- Clear line */
      term_lcf_reset(term);
      switch (CSI_DEFARG0(0)) {
        case 0: /* from cursor to end */
          cbuf_clear_region(cbuf, cpos->x, cpos->y, wdt - 1, cpos->y, defattr);
          break;
        case 1: /* from start to cursor; inclusive */
          cbuf_clear_region(cbuf, 0, cpos->y, cpos->x, cpos->y, defattr);
          break;
        case 2: /* whole line */
          cbuf_clear_region(cbuf, 0, cpos->y, wdt - 1, cpos->y, defattr);
          break;
      }
      break;
    case 'L': /* IL -- Insert <n> blank lines */
      term_lcf_reset(term);
      if (cpos->y >= stop && cpos->y <= sbot) {
        #if 0
        fprintf(stderr, "INSLINE: y=%d; top=%d; bot=%d; count=%d\n", cpos->y, stop, sbot, arg0);
        #endif
        TERM_SCTOP(term) = max2(stop, cpos->y);
        term_scroll_down(term, arg0);
        //cbuf_mark_all_dirty(TERM_CBUF(term));
        TERM_SCTOP(term) = stop;
      }
      break;
    case 'M': /* DL -- Delete <n> lines */
      term_lcf_reset(term);
      if (cpos->y >= stop && cpos->y <= sbot) {
        #if 0
        fprintf(stderr, "DELLINE: y=%d; top=%d; bot=%d; count=%d\n", cpos->y, stop, sbot, arg0);
        #endif
        // do not write to history
        const yterm_bool oldhe = term->history.enabled;
        term->history.enabled = 0;
        TERM_SCTOP(term) = max2(stop, cpos->y);
        term_scroll_up(term, arg0);
        TERM_SCTOP(term) = stop;
        term->history.enabled = oldhe;
      }
      break;
    case 'S': /* SU -- Scroll <n> line up */
      term_lcf_reset(term);
      term_scroll_up(term, arg0);
      break;
    case 'T': /* SD -- Scroll <n> line down */
      term_lcf_reset(term);
      term_scroll_down(term, arg0);
      break;
    case 'X': /* ECH -- Erase <n> char */
      term_lcf_reset(term);
      cbuf_clear_region(cbuf, cpos->x, cpos->y, cpos->x + arg0 - 1, cpos->y, defattr);
      break;
    case 'P': /* DCH -- Delete <n> char */
      term_lcf_reset(term);
      cbuf_delete_chars(cbuf, cpos->x, cpos->y, arg0, defattr);
      break;
    case 'Z': /* CBT -- Cursor Backward Tabulation <n> tab stops */
      term_lcf_reset(term);
      //FIXME: i can avoid the loop here, but meh
      {
        int nx = cpos->x;
        while (arg0 != 0 && nx != 0) {
          arg0 -= 1;
          nx = ((nx - 1) | 0x07) - 7;
        }
        if (nx != cpos->x) {
          term_invalidate_cursor(term);
          cpos->x = nx;
          cpos->justMoved = 1;
          term_invalidate_cursor(term);
        }
      }
      break;
    case 'c': // send/query device attrs
      if (term->escseq.argc == 1 && term->escseq.argv[0] <= 0) {
        term_write_raw(term, "\x1b[?1;2c", -1); // VT100
      }
      break;
    case 'g': /* TBC -- clear tab position; i don't support custom tabs */
      break;
    case 'h': /* SM -- Set terminal mode */
      for (aidx = 0; aidx < term->escseq.argc; ++aidx) {
        switch (term->escseq.argv[aidx]) {
          case 3:
            term->mode |= YTERM_MODE_DISPCTRL;
            break;
          case 4:
            term->mode |= YTERM_MODE_INSERT;
            break;
          case 20: // i don't know, it is just a wild guess
            term->mode |= YTERM_MODE_CRLF;
            break;
          default:
            csi_bad(term, aidx);
            break;
        }
      }
      break;
    case 'l': /* RM -- Reset Mode */
      for (aidx = 0; aidx < term->escseq.argc; ++aidx) {
        switch (term->escseq.argv[aidx]) {
          //2: Keyboard Action Mode (AM)
          //12: Send/receive (SRM)
          //20: Normal Linefeed (LNM)
          case 3:
            term->mode &= ~YTERM_MODE_DISPCTRL;
            break;
          case 4:
            term->mode &= ~YTERM_MODE_INSERT;
            break;
          case 20: // i don't know, it is just a wild guess
            term->mode &= ~YTERM_MODE_CRLF;
            break;
          default:
            csi_bad(term, aidx);
            break;
        }
      }
      break;
    case 'm': /* SGR -- Terminal attribute (color) */
      csi_set_attr(term, term->escseq.argv, term->escseq.argc);
      break;
    case 'n':
      switch (CSI_DEFARG0(0)) {
        case 5: /* Device status report (DSR) */
          term_write_raw(term, "\x1b[0n", -1);
          break;
        case 6: /* cursor position report */
          sprintf(buf, "\x1b[%d;%dR", cpos->y + 1, cpos->x + 1);
          term_write_raw(term, buf, -1);
          break;
      }
      break;
    case 'r': /* DECSTBM -- Set Scrolling Region */
      term_lcf_reset_nocmove(term);
      #if 0
      fprintf(stderr, "OLDREG: %d - %d\n", stop, sbot);
      #endif
      TERM_SCTOP(term) = clamp(arg0, 1, hgt) - 1;
      TERM_SCBOT(term) = clamp(CSI_DEFARG1N(1, hgt), 1, hgt) - 1;
      if (TERM_SCTOP(term) > TERM_SCBOT(term)) {
        // dunno, i think we should ignore wrong values
        TERM_SCTOP(term) = 0;
        TERM_SCBOT(term) = hgt - 1;
      }
      #if 0
      fprintf(stderr, "  NEWREG: %d - %d\n", TERM_SCTOP(term), TERM_SCBOT(term));
      #endif
      // xterm seems to do this
      term_curmove_abs(term, 0, 0);
      break;
    case 's': /* DECSC -- Save cursor position (ANSI.SYS) */
      term_save_cursor(term, SaveANSI);
      break;
    case 't': /* various emulator reports; only some of them are supported */
      if (term->escseq.argc >= 1) {
        switch (term->escseq.argv[0]) {
          case 11: // report window state
            snprintf(buf, sizeof(buf), "\x1b[%dt", (winMapped ? 1 : 2));
            term_write_raw(term, buf, -1);
            break;
          case 18: // text area size, in chars
          case 19: // screen size, in chars (wtf?!)
            snprintf(buf, sizeof(buf), "\x1b[8;%d;%dt", cbuf->height, cbuf->width);
            term_write_raw(term, buf, -1);
            break;
        }
      }
      break;
    case 'u': /* DECRC -- Restore cursor position (ANSI.SYS) */
      term_restore_cursor(term, SaveANSI);
      break;
    default:
      csi_bad(term, -1);
      break;
  }
}


//==========================================================================
//
//  csi_handle
//
//==========================================================================
static void csi_handle (Term *term) {
  const yterm_bool doit = (term->escseq.state != YTERM_ESC_CSI_IGNORE &&
                           term->escseq.cmd != 0);
  reset_esc(term); // this only resets state
  if (doit) {
    if (opt_dump_esc_enabled) {
      fprintf(stderr, "ESC-DUMP(CSI): "); dbg_csi_dump(term);
    }
    if (term->escseq.argc > YTERM_ESC_ARG_SIZ) term->escseq.argc = YTERM_ESC_ARG_SIZ;
    // fill other args with "not specified"
    for (int f = term->escseq.argc; f < YTERM_ESC_ARG_SIZ; f += 1) {
      term->escseq.argv[f] = -1;
    }
    if (term->escseq.priv < 0) { // ">"
      csi_priv_xt(term);
    } else if (term->escseq.priv > 0) { // "?"
      // don't ask me, xterm documentation says it works this way
      switch (term->escseq.cmd) {
        case 'J':
        case 'K':
          csi_normal(term);
          return;
      }
      csi_priv(term);
    } else {
      csi_normal(term);
    }
  }
}
