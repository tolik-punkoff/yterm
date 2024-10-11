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
  X11 -> terminal sequence translation tables.
  included directly into the main file.
*/


typedef struct {
  uint32_t mods;
  uint32_t ksym;
  const char *text;
  uint32_t type;
} KeyTrans;


enum {
  KB_CMD_IGNORE = -666,

  KB_CMD_PASTE_PRIMARY = 666,
  KB_CMD_PASTE_SECONDARY,
  KB_CMD_PASTE_CLIPBOARD,

  KB_CMD_START_SELECTION,

  KB_CMD_COPY_TO_PRIMARY,
  KB_CMD_COPY_TO_SECONDARY,
  KB_CMD_COPY_TO_CLIPBOARD,

  KB_CMD_TAB_PREV,
  KB_CMD_TAB_NEXT,
  KB_CMD_TAB_FIRST,
  KB_CMD_TAB_LAST,

  KB_CMD_TAB_MOVE_LEFT,
  KB_CMD_TAB_MOVE_RIGHT,

  KB_CMD_MREPORTS_TOGGLE,
  KB_CMD_MREPORTS_OFF,
  KB_CMD_MREPORTS_ON,

  KB_CMD_ESCESC_TOGGLE,
  KB_CMD_ESCESC_OFF,
  KB_CMD_ESCESC_ON,

  KB_CMD_CURSOR_TOGGLE,
  KB_CMD_CURSOR_OFF,
  KB_CMD_CURSOR_ON,

  KB_CMD_HISTORY_TOGGLE,
  KB_CMD_HISTORY_OFF,
  KB_CMD_HISTORY_ON,

  KB_CMD_COLOR_DEFAULT,
  KB_CMD_COLOR_NORMAL,
  KB_CMD_COLOR_BW,
  KB_CMD_COLOR_GREEN,

  KB_CMD_MENU_OPTIONS,
  KB_CMD_MENU_TAB_OPTIONS,
};


typedef struct KeyBind_t {
  uint32_t mods;
  uint32_t ksym;
  char *bindname;
  char *exec;
  char *path; // if path is NULL, do not change; "" is $HOME; "." is cwd
  int colorMode; // <0: do not change
  int allow_history; // <0: default
  // signal, or `KB_CMD_xxx`
  int killsignal; // signal to kill, or 0; HACK: -666 means "remove this binding"
  struct KeyBind_t *next;
} KeyBind;

static KeyBind *keybinds = NULL;


//==========================================================================
//
//  translate_keypad
//
//==========================================================================
static KeySym translate_keypad (KeySym ksym) {
  switch (ksym) {
    case XK_KP_Home: ksym = XK_Home; break;
    case XK_KP_Left: ksym = XK_Left; break;
    case XK_KP_Up: ksym = XK_Up; break;
    case XK_KP_Right: ksym = XK_Right; break;
    case XK_KP_Down: ksym = XK_Down; break;
    case XK_KP_Page_Up: ksym = XK_Page_Up; break;
    case XK_KP_Page_Down: ksym = XK_Page_Down; break;
    case XK_KP_End: ksym = XK_End; break;
    case XK_KP_Begin: ksym = XK_Begin; break;
    case XK_KP_Insert: ksym = XK_Insert; break;
    case XK_KP_Delete: ksym = XK_Delete; break;
    //case XK_KP_Equal: ksym = XK_Equal; break;
    //case XK_KP_Multiply: ksym = XK_Multiply; break;
    //case XK_KP_Add: ksym = XK_Add; break;
    //case XK_KP_Separator: ksym = XK_Separator; break;
    //case XK_KP_Subtract: ksym = XK_Subtract; break;
    //case XK_KP_Decimal: ksym = XK_Decimal; break;
    //case XK_KP_Divide: ksym = XK_Divide; break;
    case XK_KP_Enter: ksym = XK_Return; break;
  }
  return ksym;
}


#include "x11_keymap.inc.c"
#include "menus.inc.c"


//==========================================================================
//
//  term_remove_from_list
//
//==========================================================================
static void term_remove_from_list (Term *term) {
  if (term->prev != NULL) term->prev->next = term->next; else termlist = term->next;
  if (term->next != NULL) term->next->prev = term->prev;
  term->prev = NULL;
  term->next = NULL;
}


//==========================================================================
//
//  term_insert_after
//
//  if `after` is `NULL`, insert as the first one
//
//==========================================================================
static void term_insert_after (Term *term, Term *after) {
  yterm_assert(term->prev == NULL && term->next == NULL);
  if (after == NULL) {
    term->next = termlist;
    if (termlist != NULL) termlist->prev = term;
    termlist = term;
  } else {
    term->next = after->next;
    term->prev = after;
    if (after->next != NULL) after->next->prev = term;
    after->next = term;
  }
}


//==========================================================================
//
//  do_keybind
//
//==========================================================================
static yterm_bool do_keybind (Term *term, KeyBind *kb) {
  if (term == NULL) return 1;

  #if 0
  fprintf(stderr, "KEYBIND: %s\n", kb->bindname);
  #endif

  switch (kb->killsignal) {
    case KB_CMD_PASTE_PRIMARY:
      if (term->deadstate == DS_ALIVE) do_paste(term, 0);
      return 1;
    case KB_CMD_PASTE_SECONDARY:
      if (term->deadstate == DS_ALIVE) do_paste(term, 1);
      return 1;
    case KB_CMD_PASTE_CLIPBOARD:
      if (term->deadstate == DS_ALIVE) do_paste(term, 2);
      return 1;

    // those are active only in selection mode
    case KB_CMD_COPY_TO_PRIMARY:
    case KB_CMD_COPY_TO_SECONDARY:
    case KB_CMD_COPY_TO_CLIPBOARD:
      return 0;

    case KB_CMD_START_SELECTION:
      term->history.blocktype = SBLOCK_STREAM;
      term->history.locked = 0;
      term->history.cx = TERM_CPOS(term)->x;
      term->history.cy = TERM_CPOS(term)->y;
      term->history.sback = 0;
      term->history.x0 = 0; term->history.y0 = 0;
      term->history.x1 = -1; term->history.y1 = -1;
      term->history.inprogress = 0;
      cbuf_free(&hvbuf);
      cbuf_new(&hvbuf, TERM_CBUF(term)->width, TERM_CBUF(term)->height);
      memcpy(hvbuf.buf, TERM_CBUF(term)->buf,
             (unsigned)(TERM_CBUF(term)->width * TERM_CBUF(term)->height) * sizeof(CharCell));
      hvbuf.dirtyCount = TERM_CBUF(term)->dirtyCount;
      cbuf_mark_all_dirty(&hvbuf);
      return 1;

    case KB_CMD_TAB_PREV:
      term = term->prev;
      while (term != NULL && term->deadstate == DS_DEAD) term = term->prev;
      if (opt_cycle_tabs && term == NULL) {
        term = termlist;
        while (term->next != NULL) term = term->next;
        while (term != NULL && term->deadstate == DS_DEAD) term = term->prev;
      }
      term_activate(term);
      return 1;
    case KB_CMD_TAB_NEXT:
      term = term->next;
      while (term != NULL && term->deadstate == DS_DEAD) term = term->next;
      if (opt_cycle_tabs && term == NULL) {
        term = termlist;
        while (term != NULL && term->deadstate == DS_DEAD) term = term->next;
      }
      term_activate(term);
      return 1;

    case KB_CMD_TAB_FIRST:
      term = termlist;
      while (term != NULL && term->deadstate == DS_DEAD) term = term->next;
      term_activate(term);
      return 1;
    case KB_CMD_TAB_LAST:
      term = termlist;
      if (term != NULL) {
        while (term->next != NULL) term = term->next;
        while (term != NULL && term->deadstate == DS_DEAD) term = term->prev;
        term_activate(term);
      }
      return 1;

    case KB_CMD_TAB_MOVE_LEFT:
      if (term->prev != NULL) {
        Term *pt = term->prev;
        term_remove_from_list(term);
        term_insert_after(term, pt->prev);
        force_tab_redraw();
      }
      return 1;
    case KB_CMD_TAB_MOVE_RIGHT:
      if (term->next != NULL) {
        Term *nt = term->next;
        term_remove_from_list(term);
        term_insert_after(term, nt);
        force_tab_redraw();
      }
      return 1;

    case KB_CMD_MREPORTS_TOGGLE:
      if (term->mouseReports < 0) {
        term->mouseReports = !opt_mouse_reports;
      } else {
        term->mouseReports = !term->mouseReports;
      }
      if (term->mouseReports) {
        term_osd_message(term, "tab mouse reports: tan");
      } else {
        term_osd_message(term, "tab mouse reports: ona");
      }
      return 1;
    case KB_CMD_MREPORTS_OFF:
      term->mouseReports = 0;
      term_osd_message(term, "tab mouse reports: ona");
      return 1;
    case KB_CMD_MREPORTS_ON:
      term->mouseReports = 1;
      term_osd_message(term, "tab mouse reports: tan");
      return 1;

    case KB_CMD_ESCESC_TOGGLE:
      if (term->escesc < 0) {
        term->escesc = !opt_esc_twice;
      } else {
        term->escesc = !term->escesc;
      }
      if (term->escesc) {
        term_osd_message(term, "tab esc-esc: tan");
      } else {
        term_osd_message(term, "tab esc-esc: ona");
      }
      return 1;
    case KB_CMD_ESCESC_OFF:
      term->escesc = 0;
      term_osd_message(term, "tab esc-esc: ona");
      return 1;
    case KB_CMD_ESCESC_ON:
      term->escesc = 1;
      term_osd_message(term, "tab esc-esc: tan");
      return 1;

    case KB_CMD_CURSOR_TOGGLE:
      term_invalidate_cursor(term);
      term->wkscr->curhidden = !term->wkscr->curhidden;
      term_invalidate_cursor(term);
      return 1;
    case KB_CMD_CURSOR_OFF:
      term_invalidate_cursor(term);
      term->wkscr->curhidden = 1;
      term_invalidate_cursor(term);
      return 1;
    case KB_CMD_CURSOR_ON:
      term_invalidate_cursor(term);
      term->wkscr->curhidden = 0;
      term_invalidate_cursor(term);
      return 1;

    case KB_CMD_HISTORY_TOGGLE:
      term->history.enabled = !term->history.enabled;
      goto histdone;
    case KB_CMD_HISTORY_OFF:
     term->history.enabled = 0;
      goto histdone;
    case KB_CMD_HISTORY_ON:
      term->history.enabled = 0;
      histdone:
        if (term->history.enabled) {
          term_osd_message(term, "history enabled.");
        } else {
          term_osd_message(term, "history disabled.");
          history_close(&term->history.file);
          term->history.file.width = TERM_CBUF(term)->width;
          if (term->history.blocktype != SBLOCK_NONE) {
            term->history.blocktype = SBLOCK_NONE;
            cbuf_mark_all_dirty(TERM_CBUF(term));
          }
        }
      break;

    case KB_CMD_COLOR_DEFAULT:
      term->colorMode = -1;
      cbuf_mark_all_dirty(TERM_CBUF(term));
      break;
    case KB_CMD_COLOR_NORMAL:
      term->colorMode = CMODE_NORMAL;
      cbuf_mark_all_dirty(TERM_CBUF(term));
      break;
    case KB_CMD_COLOR_BW:
      term->colorMode = CMODE_BW;
      cbuf_mark_all_dirty(TERM_CBUF(term));
      break;
    case KB_CMD_COLOR_GREEN:
      term->colorMode = CMODE_GREEN;
      cbuf_mark_all_dirty(TERM_CBUF(term));
      break;

    case KB_CMD_MENU_OPTIONS:
      create_global_options_menu(0, 0);
      return 1;
    case KB_CMD_MENU_TAB_OPTIONS:
      create_tab_options_menu(0, 0);
      return 1;
  }

  if (kb->killsignal > 0) {
    if (kb->killsignal < 255) {
      if (term->deadstate == DS_ALIVE && term->child.cmdfd >= 0) {
        pid_t pgrp = tcgetpgrp(term->child.cmdfd);
        if (pgrp > 0) kill(pgrp, kb->killsignal);
      }
    }
    return 1;
  }

  // it must be exec
  if (kb->exec != NULL) {
    if (!opt_enable_tabs) return 1;

    ExecData ed;
    if (execsh_prepare(&ed, kb->exec) == 0) {
      fprintf(stderr, "ERROR: cannot parse exec line: '%s'!\n", kb->exec);
      return 0;
    }

    // setup starup dir
    const char *newdir = NULL;
    if (kb->path == NULL || kb->path[0] == 0 || strcmp(kb->path, "$PWD") == 0) {
      pid_t pgrp = 0;
      if (currterm != NULL && currterm->child.cmdfd >= 0) {
        pgrp = tcgetpgrp(term->child.cmdfd);
      }
      if (pgrp > 0) {
        // get program current dir
        snprintf(txrdp, sizeof(txrdp), "/proc/%d/cwd", (int)pgrp);
        ssize_t rd = readlink(txrdp, txpath, sizeof(txpath) - 1);
        if (rd > 0) {
          txpath[rd] = 0;
          newdir = txpath;
        }
      }
    } else if (strcmp(kb->path, "$HOME") == 0) {
      newdir = getenv("HOME");
      if (newdir == NULL || newdir[0] == 0) newdir = "/home";
    } else if (kb->path[0] == '/') {
      newdir = kb->path;
    }

    if (newdir != NULL && newdir[0] != 0) {
      #if 0
      fprintf(stderr, "NEWDIR: <%s>\n", newdir);
      #endif
      free(ed.path);
      ed.path = strdup(newdir);
      if (ed.path == NULL) {
        execsh_free(&ed);
        fprintf(stderr, "ERROR: out of memory!\n");
        return 1;
      }
    }

    Term *newterm = term_create_new_term();
    if (newterm == NULL) { execsh_free(&ed); return 1; }
    if (kb->allow_history >= 0) newterm->history.enabled = !!kb->allow_history;

    const yterm_bool res = term_exec(newterm, &ed);
    execsh_free(&ed);

    if (res == 0) {
      newterm->deadstate = DS_DEAD;
    } else {
      if (kb->colorMode >= 0) newterm->colorMode = kb->colorMode;
      term_activate(newterm);
    }

    return 1;
  }

  // there's something's strange in the neighbourhood
  return 0;
}


//==========================================================================
//
//  is_good_ttype
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool is_good_ttype (uint32_t ttype) {
  return (ttype == TT_ANY || ttype == (unsigned)opt_term_type);
}


#include "selection.inc.c"


//==========================================================================
//
//  mods_to_num
//
//==========================================================================
YTERM_STATIC_INLINE int mods_to_num (uint32_t kmods) {
  char res = 0;
  if (kmods & Mod_Shift) res |= 1;
  if (kmods & Mod_Alt) res |= 2;
  if (kmods & Mod_Ctrl) res |= 4;
  if (res != 0) res += 1;
  return res;
}


//==========================================================================
//
//  x11_key_modkey
//
//==========================================================================
static void x11_key_modkey (KeySym ksym, yterm_bool down) {
  Term *term = currterm;
  if (term != NULL) {
    uint32_t mask = 0;
    switch (ksym) {
      case XK_Alt_L: mask = MKey_Alt_L; break;
      case XK_Alt_R: mask = MKey_Alt_R; break;
      case XK_Control_L: mask = MKey_Ctrl_L; break;
      case XK_Control_R: mask = MKey_Ctrl_R; break;
      case XK_Shift_L: mask = MKey_Shift_L; break;
      case XK_Shift_R: mask = MKey_Shift_R; break;
    }
    if (mask != 0) {
      if (down) term->lastModKeys |= mask; else term->lastModKeys &= ~mask;
    }
  }
}


//==========================================================================
//
//  x11_build_lmstate
//
//==========================================================================
YTERM_STATIC_INLINE uint32_t x11_build_lmstate (uint32_t lastMods) {
  uint32_t mc = 0;
  if (lastMods & (MKey_Shift_L | MKey_Shift_R)) mc |= 1;
  if (lastMods & (MKey_Alt_L | MKey_Alt_R)) mc |= 2;
  if (lastMods & (MKey_Ctrl_L | MKey_Ctrl_R)) mc |= 4;
  if (mc != 0) mc += 1;
  return mc;
}


//==========================================================================
//
//  x11_send_modifiers_extcode
//
//==========================================================================
static void x11_send_modifiers_extcode (KeySym ksym, yterm_bool down) {
  char sbuf[32];
  Term *term = currterm;
  if (term != NULL) {
    const uint32_t lastMods = x11_build_lmstate(term->lastModKeys);
    x11_key_modkey(ksym, down);
    const uint32_t newMods = x11_build_lmstate(term->lastModKeys);
    if (lastMods != newMods && term->reportMods && /*curr_menu == NULL &&*/
        !TERM_IN_SELECTION(term) && term->deadstate == DS_ALIVE)
    {
      if (newMods != 0) {
        snprintf(sbuf, sizeof(sbuf), "\x1b[1;%dY", (int)newMods);
      } else {
        snprintf(sbuf, sizeof(sbuf), "\x1b[1Y");
      }
      term_write_raw(term, sbuf, strlen(sbuf));
    }
  }
}


//==========================================================================
//
//  x11_parse_keyevent_release
//
//==========================================================================
static void x11_parse_keyevent_release (XKeyEvent *e) {
  KeySym ksym = XLookupKeysym(e, 0);
  switch (ksym) {
    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Control_L:
    case XK_Control_R:
    case XK_Shift_L:
    case XK_Shift_R:
    case XK_Super_L:
    case XK_Super_R:
    case XK_Hyper_L:
    case XK_Hyper_R:
      x11_send_modifiers_extcode(ksym, 0);
      return;
  }
}


//==========================================================================
//
//  x11_parse_keyevent
//
//==========================================================================
static void x11_parse_keyevent (XKeyEvent *e) {
  char readkeybuf[128];
  Term *term = currterm;
  Status status = 0;
  KeySym ksym = XLookupKeysym(e, 0);
  int mc, ascii;

  readkeybuf[0] = 0;

  switch (ksym) {
    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Control_L:
    case XK_Control_R:
    case XK_Shift_L:
    case XK_Shift_R:
    case XK_Super_L:
    case XK_Super_R:
    case XK_Hyper_L:
    case XK_Hyper_R:
      x11_send_modifiers_extcode(ksym, 1);
      return;
  }

  const uint32_t kmods = (e->state & (Mod_Ctrl | Mod_Shift | Mod_Alt /*| Mod_AltGr*/ | Mod_Hyper));
  // remove unused modifiers
  const int numlock = !!(e->state&Mod2Mask);

  #if 0
  {
    const char *ksname = XKeysymToString(ksym);
    fprintf(stderr, "PRESSED: ");
    if (kmods & Mod_Ctrl) fprintf(stderr, "C-");
    if (kmods & Mod_Alt) fprintf(stderr, "M-");
    if (kmods & Mod_Hyper) fprintf(stderr, "H-");
    if (kmods & Mod_Shift) fprintf(stderr, "S-");
    fprintf(stderr, "%s\n", ksname);
  }
  #endif

  // OSD menu navigation
  if (curr_menu != NULL) {
    x11_menu_keyevent(e, ksym, kmods);
    return;
  }

  // selection control
  if (TERM_IN_SELECTION(term)) {
    x11_parse_keyevent_selection(term, e, ksym, kmods);
    return;
  }

  // check keybinds (for dead tabs too)
  //fprintf(stderr, "KS: %s\n", XKeysymToString(ksym));
  for (KeyBind *kb = keybinds; kb != NULL; kb = kb->next) {
    if (kb->mods == kmods && kb->ksym == ksym) {
      if (do_keybind(term, kb)) return;
    }
  }

  // dead tab waiting for a keypress
  if (term->deadstate == DS_WAIT_KEY) {
    switch (ksym) {
      case XK_Return:
      case XK_KP_Enter:
      case XK_space:
        term->deadstate = DS_DEAD;
        break;
    }
  }

  if (term->deadstate == DS_WAIT_KEY_ESC) {
    switch (ksym) {
      case XK_Escape:
        term->deadstate = DS_DEAD;
        break;
    }
  }

  if (term->deadstate != DS_ALIVE) return;

  #ifdef USELESS_DEBUG_FEATURES
  // dump two screens
  if (ksym == XK_F1 && kmods == Mod_Hyper) {
    if (opt_dump_fd >= 0) {
      opt_dump_fd_enabled = !opt_dump_fd_enabled;
      if (opt_dump_fd_enabled) {
        fprintf(stderr, "DEBUG: write logs enabled.\n");
      } else {
        fprintf(stderr, "DEBUG: write logs disabled.\n");
      }
    } else {
      FILE *fo = fopen("z_scr0.bin", "w");
      fwrite(term->main.cbuf.buf, sizeof(CharCell), term->main.cbuf.width * term->main.cbuf.height, fo);
      fclose(fo);

      fo = fopen("z_scr1.bin", "w");
      fwrite(term->alt.cbuf.buf, sizeof(CharCell), term->alt.cbuf.width * term->alt.cbuf.height, fo);
      fclose(fo);

      fprintf(stderr, "DEBUG: screens dumped.\n");
    }
    return;
  }
  // dump esc sequences
  if (ksym == XK_F2 && kmods == Mod_Hyper) {
    opt_dump_esc_enabled = !opt_dump_esc_enabled;
    if (opt_dump_esc_enabled) {
      fprintf(stderr, "DEBUG: escape sequences dump enabled.\n");
    } else {
      fprintf(stderr, "DEBUG: escape sequences dump disabled.\n");
    }
    return;
  }
  // slow it down
  if (ksym == XK_F9 && kmods == Mod_Hyper) {
    opt_debug_slowdown = !opt_debug_slowdown;
    if (opt_debug_slowdown) {
      fprintf(stderr, "DEBUG: slowdown enabled.\n");
    } else {
      fprintf(stderr, "DEBUG: slowdown disabled.\n");
    }
    return;
  }
  #endif

  // xterm-like "Home" and "End"
  if (kmods == 0 && (term->mode & YTERM_MODE_XTERM) != 0 &&
      (ksym == XK_Home || ksym == XK_End ||
       (!numlock && (ksym == XK_KP_Home || ksym == XK_KP_End))))
  {
    if (ksym == XK_Home || ksym == XK_KP_Home) {
      if ((term->mode & YTERM_MODE_VT220_KBD) != 0) {
        term_write_raw(term, "\x1b[H", -1);
      } else {
        term_write_raw(term, "\x1b[1~", -1);
      }
    } else {
      if ((term->mode & YTERM_MODE_VT220_KBD) != 0) {
        term_write_raw(term, "\x1b[F", -1);
      } else {
        term_write_raw(term, "\x1b[4~", -1);
      }
    }
    return;
  }

  // esc-esc mode?
  if (ksym == XK_Escape && (kmods == 0 || kmods == Mod_Alt)) {
    yterm_bool twice = (term->escesc == 1 || (term->escesc < 0 && opt_esc_twice));
    if (kmods & Mod_Alt) twice = !twice;
    if (twice) {
      term_write_raw(term, "\x1b\x1b", 2);
    } else {
      term_write_raw(term, "\x1b", 1);
    }
    return;
  }

  // some keypad chars (mul, div, etc)
  // shift sends the normal char
  // (use "cat -v" to check keyboard codes in various emulators)
  #if 0
  if (opt_term_type == TT_RXVT && (kmods & Mod_Shift) == 0) {
    switch (ksym) {
      case XK_KP_Divide:
        if (kmods & Mod_Alt) term_write_raw(term, "\x1b\x1bOo", -1);
        else term_write_raw(term, "\x1bOo", -1);
        return;
      case XK_KP_Multiply:
        if (kmods & Mod_Alt) term_write_raw(term, "\x1b\x1bOj", -1);
        else term_write_raw(term, "\x1bOj", -1);
        return;
      case XK_KP_Subtract:
        if (kmods & Mod_Alt) term_write_raw(term, "\x1b\x1bOm", -1);
        else term_write_raw(term, "\x1bOm", -1);
        return;
      case XK_KP_Add:
        if (kmods & Mod_Alt) term_write_raw(term, "\x1b\x1bOk", -1);
        else term_write_raw(term, "\x1bOk", -1);
        return;
      /* nope, i don't need it
      case XK_KP_Enter:
        if (kmods & Mod_Alt) term_write_raw(term, "\x1b\x1bOM", -1);
        else term_write_raw(term, "\x1bOM", -1);
        break;
      */
    }
  }
  #endif

  // check keymap
  for (const KeyTrans *ktr = keymap; ktr->text != NULL; ktr += 1) {
    if (is_good_ttype(ktr->type) && ktr->mods == kmods && ktr->ksym == ksym) {
      // i found her!
      term_write(term, ktr->text);
      return;
    }
  }

  // C+space
  if (ksym == XK_space && (kmods & Mod_Ctrl) != 0) {
    term_write_raw(term, "\x00", 1);
    return;
  }

  // yterm extension
  // various mods with alphanum keys: "\x1b[<upcase-ascii-code>;modY"
  mc = mods_to_num(kmods);
  ascii = 0;
  #if 0
  if (mc != 0) fprintf(stderr, "*** mc=%d\n", mc);
  #endif
  if (mc == 4 || mc >= 6) {
    if (ksym >= XK_a && ksym <= XK_z) ascii = (int)ksym - (int)XK_a + 'A';
    else if (ksym >= XK_A && ksym <= XK_Z) ascii = (int)ksym - (int)XK_A + 'A';
    else if (ksym >= XK_0 && ksym <= XK_9) ascii = (int)ksym - (int)XK_0 + '0';
  } else if (/*mc == 2 ||*/ mc >= 4) {
    if (ksym >= XK_0 && ksym <= XK_9) ascii = (int)ksym - (int)XK_0 + '0';
  }
  if (ascii != 0) {
    // "\x1b[<upcase-ascii-code>;modY"
    #if 0
    fprintf(stderr, "*** ASCII=%d; mc=%d\n", ascii, mc);
    #endif
    snprintf(readkeybuf, sizeof(readkeybuf), "\x1b[%d;%dY", ascii, mc);
    term_write_raw(term, readkeybuf, strlen(readkeybuf));
    return;
  }

  // try translated keypad too
  yterm_bool keymap_again = 0;
  if (!numlock) {
    KeySym newksym = translate_keypad(ksym);
    if (newksym != ksym) {
      ksym = newksym;
      keymap_again = 1;
    }
  } else if (ksym == XK_KP_Enter) {
    ksym = XK_Return;
    keymap_again = 1;
  }

  if (keymap_again) {
    for (const KeyTrans *ktr = keymap; ktr->text != NULL; ktr += 1) {
      if (is_good_ttype(ktr->type) && ktr->mods == kmods && ktr->ksym == ksym) {
        // i found her!
        term_write(term, ktr->text);
        return;
      }
    }
  }

  // is this something with ctrl?
  const int meta = (e->state&Mod1Mask);
  const int shift = (e->state&ShiftMask);

  if (ksym == XK_Return || ksym == XK_KP_Enter) {
    if (meta) {
      term_write_raw(term, "\x1b\x0a", 2);
    } else {
      if (term->mode & YTERM_MODE_CRLF) {
        term_write_raw(term, "\x0d\x0a", 2);
      } else {
        term_write_raw(term, "\x0d", 1);
      }
    }
  } else {
    int len;
    if (x11_xic) {
      len = Xutf8LookupString(x11_xic, e, readkeybuf, (int)sizeof(readkeybuf)-2, NULL, &status);
      if (len < 0) len = 0;
    } else {
      char tempbuf[16];
      len = XLookupString(e, tempbuf, (int)sizeof(tempbuf)-1, NULL, NULL);
      // convert to UTF8
      if (len < 0) len = 0;
      //TODO: other locales
      int dp = 0, spos = 0;
      while (spos < len) {
        uint8_t uch = ((const unsigned char *)tempbuf)[spos]; spos += 1;
        if (uch <= 128) {
          if (dp < (int)sizeof(readkeybuf) - 2) {
            readkeybuf[dp] = (char)uch; dp += 1;
          } else {
            len = 0;
          }
        } else if (koiLocale) {
          char ubuf[4];
          uint32_t cp = yterm_koi2uni(uch);
          uint32_t ulen = yterm_utf8_encode(ubuf, cp);
          if (dp + (int)ulen <= (int)sizeof(readkeybuf) - 2) {
            memcpy(readkeybuf + dp, ubuf, ulen);
            dp += (int)ulen;
          } else {
            len = 0;
          }
        }
      }
      yterm_assert(dp >= 0 && dp < (int)sizeof(readkeybuf) - 2);
      len = dp;
    }
    readkeybuf[len] = 0;

    if (len > 0) {
      //KeySym ksx = (meta ? XLookupKeysym(e, 0) : NoSymbol);
      if (meta && ((ksym >= 'A' && ksym <= 'Z') || (ksym >= 'a' && ksym <= 'z'))) {
        // alt+key: ignore current xcb language
        // but with shift key we have to transform case
        if (shift) {
          if (ksym >= 'a' && ksym <= 'z') ksym -= 32; // upcase
        } else {
          if (ksym >= 'A' && ksym <= 'Z') ksym += 32; // locase
        }
        readkeybuf[0] = '\x1b';
        readkeybuf[1] = (char)ksym;
        readkeybuf[2] = 0;
        term_write(term, readkeybuf);
      } else {
        if (meta && len == 1) {
          // prepend esc
          yterm_assert(len + 1 < (int)sizeof(readkeybuf));
          memmove(readkeybuf + 1, readkeybuf, len + 1);
          readkeybuf[0] = 0x1b;
        }
        term_write(term, readkeybuf);
      }
    }
  }
}
