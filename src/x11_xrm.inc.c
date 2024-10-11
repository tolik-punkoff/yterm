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
  Xrdb options parsing.
  included directly into the main file.
*/


#define XRM_BAD_COLOR  (0xffffffffU)

static char xrm_key_value[1024];


//==========================================================================
//
//  strEquCI
//
//==========================================================================
static yterm_bool strEquCI (const char *s0, const char *s1) {
  if (s0 == NULL) s0 = "";
  if (s1 == NULL) s1 = "";
  while (*s0 != 0 && *s1 != 0) {
    char c0 = *s0++;
    if (c0 >= 'A' && c0 <= 'Z') c0 = c0 - 'A' + 'a';
    char c1 = *s1++;
    if (c1 >= 'A' && c1 <= 'Z') c1 = c1 - 'A' + 'a';
    if (c0 != c1) return 0;
  }
  return (s0[0] == 0 && s1[0] == 0);
}


//==========================================================================
//
//  parse_keybind
//
//==========================================================================
static yterm_bool parse_keybind (KeyBind *kb, const char *str) {
  yterm_bool res = 0;
  KeySym keysym = NoSymbol;
  uint32_t modmask = 0;
  while (*str > 0 && *str <= 32) str += 1;
  if (str && str[0]) {
    while (*str) {
      if (str[0] == 'C' && str[1] == '-') {
        modmask = modmask | ControlMask;
        str += 2;
      } else if (str[0] == 'M' && str[1] == '-') {
        modmask = modmask | Mod1Mask;
        str += 2;
      } else if (str[0] == 'H' && str[1] == '-') {
        modmask = modmask | Mod4Mask;
        str += 2;
      } else if (str[0] == 'S' && str[1] == '-') {
        modmask = modmask | ShiftMask;
        str += 2;
      } else {
        break;
      }
    }
    if (str[0]) {
      keysym = XStringToKeysym(str);
      // this is what X.org returns from `XLookupKeysym()` even if shift is pressed
      if (keysym >= XK_A && keysym <= XK_Z) keysym = keysym - XK_A + XK_a;
    }
  }

  if (keysym != NoSymbol) {
    if (kb) {
      kb->mods = modmask;
      kb->ksym = keysym;
      res = 1;
    }
  }

  return res;
}


//==========================================================================
//
//  parse_signal_name
//
//==========================================================================
static int parse_signal_name (const char *str) {
  if ((str[0] == 's' || str[0] == 'S') &&
      (str[1] == 'i' || str[1] == 'I') &&
      (str[2] == 'g' || str[2] == 'G'))
  {
    str += 3;
  }
  while (*str == '_') str += 1;
  if (strEquCI(str, "HUP")) return 1;
  if (strEquCI(str, "INT")) return 2;
  if (strEquCI(str, "QUIT")) return 3;
  if (strEquCI(str, "ILL")) return 4;
  if (strEquCI(str, "TRAP")) return 5;
  if (strEquCI(str, "ABRT")) return 6;
  if (strEquCI(str, "ABORT")) return 6;
  //if (strEquCI(str, "IOT")) return 6;
  if (strEquCI(str, "BUS")) return 7;
  if (strEquCI(str, "FPE")) return 8;
  if (strEquCI(str, "KILL")) return 9;
  if (strEquCI(str, "USR1")) return 10;
  if (strEquCI(str, "SEGV")) return 11;
  if (strEquCI(str, "USR2")) return 12;
  //if (strEquCI(str, "PIPE")) return 13;
  if (strEquCI(str, "ALRM")) return 14;
  if (strEquCI(str, "ALARM")) return 14;
  if (strEquCI(str, "TERM")) return 15;
  if (strEquCI(str, "STKFLT")) return 16;
  //if (strEquCI(str, "CHLD")) return 17;
  if (strEquCI(str, "CONT")) return 18;
  if (strEquCI(str, "STOP")) return 19;
  //if (strEquCI(str, "TSTP")) return 20;
  //if (strEquCI(str, "TTIN")) return 21;
  //if (strEquCI(str, "TTOU")) return 22;
  if (strEquCI(str, "URG")) return 23;
  //if (strEquCI(str, "XCPU")) return 24;
  //if (strEquCI(str, "XFSZ")) return 25;
  //if (strEquCI(str, "VTALRM")) return 26;
  //if (strEquCI(str, "PROF")) return 27;
  //if (strEquCI(str, "WINCH")) return 28;
  return 0;
}


//==========================================================================
//
//  xrm_val_to_val
//
//==========================================================================
static char *xrm_val_to_val (XrmValue *val) {
  char *res = NULL;
  if (val != NULL) {
    const char *str = (const char *)val->addr;
    size_t slen = (val->size > 0 && str != NULL ? strlen(str) : 0);
    // trim leading spaces
    while (slen > 0 && str[0] > 0 && str[0] <= 32) {
      str += 1; slen -= 1;
    }
    // trim trailing spaces
    while (slen > 0 && str[slen - 1] > 0 && str[slen - 1] <= 32) {
      slen -= 1;
    }
    if (slen > 0 && slen < sizeof(xrm_key_value)) {
      memcpy(xrm_key_value, str, slen);
      xrm_key_value[slen] = 0;
      res = xrm_key_value;
    }
  }
  return res;
}


//==========================================================================
//
//  parse_bool
//
//==========================================================================
static int parse_bool (const char *val) {
  int res = -1;
  if (val != NULL) {
    if (strEquCI(val, "tan")) res = 1;
    else if (strEquCI(val, "yes")) res = 1;
    else if (strEquCI(val, "true")) res = 1;
    else if (strEquCI(val, "on")) res = 1;
    else if (strEquCI(val, "1")) res = 1;
    else if (strEquCI(val, "ona")) res = 0;
    else if (strEquCI(val, "no")) res = 0;
    else if (strEquCI(val, "false")) res = 0;
    else if (strEquCI(val, "off")) res = 0;
    else if (strEquCI(val, "0")) res = 0;
  }
  return res;
}


//==========================================================================
//
//  hex_digit
//
//==========================================================================
static int hex_digit (char ch) {
  return
    ch >= '0' && ch <= '9' ? ch - '0' :
    ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 :
    ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
    -1;
}


//==========================================================================
//
//  parse_hex_color
//
//==========================================================================
static uint32_t parse_hex_color (const char *cname) {
  if (cname == NULL) return XRM_BAD_COLOR;
  while (*cname > 0 && *cname <= 32) cname += 1;
  if (cname[0] != '#') return XRM_BAD_COLOR;
  cname += 1;
  while (*cname > 0 && *cname <= 32) cname += 1;
  uint32_t vals[6] = {0, 0, 0, 0, 0, 0};
  int ccnt = 0;
  while (ccnt < 6 && *cname > 32) {
    const int dig = hex_digit(*cname); cname += 1;
    if (dig < 0) return XRM_BAD_COLOR;
    vals[ccnt] = (unsigned)dig;
    ccnt += 1;
  }
  if (ccnt != 3 && ccnt != 6) return XRM_BAD_COLOR;
  while (*cname > 0 && *cname <= 32) cname += 1;
  if (cname[0]) return XRM_BAD_COLOR;
  if (ccnt == 3) {
    vals[4] = vals[2]; vals[5] = vals[2];
    vals[2] = vals[1]; vals[3] = vals[1];
    vals[1] = vals[0];
  }
  return
    (vals[0] << 20) | (vals[1] << 16) |
    (vals[2] << 12) | (vals[3] << 8) |
    (vals[4] << 4) | vals[5];
}


// ////////////////////////////////////////////////////////////////////////// //
static char xrm_errmenu_str[128];
static int xrm_errmenu_strpos = 0;


//==========================================================================
//
//  xrm_errmenu_put
//
//==========================================================================
static void xrm_errmenu_put (const char *str) {
  if (str && str[0]) {
    int slen = (int)strlen(str);
    if (xrm_errmenu_strpos + slen >= (int)sizeof(xrm_errmenu_str) - 1 - 4) {
      xrm_errmenu_str[xrm_errmenu_strpos] = '.'; xrm_errmenu_strpos += 1;
      xrm_errmenu_str[xrm_errmenu_strpos] = '.'; xrm_errmenu_strpos += 1;
      xrm_errmenu_str[xrm_errmenu_strpos] = '.'; xrm_errmenu_strpos += 1;
      xrm_errmenu_str[xrm_errmenu_strpos] = 0;
      xrm_errmenu_strpos = (int)sizeof(xrm_errmenu_str);
    } else {
      strcpy(xrm_errmenu_str + xrm_errmenu_strpos, str);
      xrm_errmenu_strpos += slen;
    }
  }
}


//==========================================================================
//
//  show_error_menu
//
//==========================================================================
static void show_error_menu (const char *errmsg, XrmQuarkList quarks) {
  MenuWindow *menu = menu_new_window(0, 0, "Config Error!", 0, 0);

  // error message
  xrm_errmenu_strpos = 0; xrm_errmenu_str[0] = 0;
  xrm_errmenu_put("Error: ");
  xrm_errmenu_put(errmsg);
  menu_add_label(menu, xrm_errmenu_str, NULL);

  // key
  xrm_errmenu_strpos = 0; xrm_errmenu_str[0] = 0;
  xrm_errmenu_put("  Key: ");
  int f = 0;
  while (quarks[f] != NULLQUARK) {
    const char *qq = XrmQuarkToString(quarks[f]);
    if (f != 0) xrm_errmenu_put(".");
    xrm_errmenu_put(qq);
    f += 1;
  }
  menu_add_label(menu, xrm_errmenu_str, NULL);
  menu_add_label(menu, NULL, NULL);

  menu->curr_item = menu->items_count;
  menu_add_label(menu, "&Close", &menu_msgbox_close_cb);
  menu_center(menu);
  open_menu(menu);
}


//==========================================================================
//
//  xrm_keybind_cb
//
//==========================================================================
static Bool xrm_keybind_cb (XrmDatabase *db, XrmBindingList bindings, XrmQuarkList quarks,
                            XrmRepresentation *type, XrmValue *value, XPointer udata)
{
  (void)db; (void)bindings; (void)quarks; (void)type; (void)value; (void)udata;
  //fprintf(stderr, "=====\n");

  const char *errmsg = NULL;

  if (quarks[0] == NULLQUARK || strcmp(XrmQuarkToString(quarks[0]), xrm_app_name) != 0) return False;
  if (quarks[1] == NULLQUARK || strcmp(XrmQuarkToString(quarks[1]), "keybind") != 0) {
    errmsg = "invalid key name";
    goto error;
  }
  if (quarks[2] == NULLQUARK || quarks[3] == NULLQUARK || quarks[4] != NULLQUARK) {
    errmsg = "invalid key format";
    goto error;
  }

  if (!type || strcmp(XrmQuarkToString(*type), "String") != 0) {
    errmsg = "invalid key type";
    goto error;
  }

  if (value->size == 0 || value->addr == NULL) {
    errmsg = "empty key value";
    goto error;
  }

  KeyBind *kb = keybinds;
  while (kb != NULL && strcmp(kb->bindname, XrmQuarkToString(quarks[2])) != 0) {
    kb = kb->next;
  }

  if (!kb) {
    // create new empty keybind
    kb = malloc(sizeof(KeyBind));
    if (!kb) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
    memset(kb, 0, sizeof(KeyBind));
    kb->ksym = NoSymbol;
    kb->bindname = strdup(XrmQuarkToString(quarks[2]));
    kb->colorMode = -1;
    kb->killsignal = 0;
    kb->allow_history = -1;
    kb->next = keybinds;
    keybinds = kb;
  }

  const char *key = XrmQuarkToString(quarks[3]);
  const char *val = (const char *)value->addr;

  #if 0
  {
    fprintf(stderr, "name: ");
    int qc = 0;
    while (quarks[qc] != NULLQUARK) {
      const char *qs = XrmQuarkToString(quarks[qc]);
      if (qc != 0) fputc('.', stderr);
      fprintf(stderr, "%s", qs);
      qc += 1;
    }
    fputc('\n', stderr);
    fprintf(stderr, "  key: %s\n", key);
    fprintf(stderr, "  value: %s\n", val);
  }
  #endif

  if (strcmp(key, "bind") == 0) {
    // key binding, parse it
    if (strEquCI(val, "none") || strEquCI(val, "disable") ||
        strEquCI(val, "disabled"))
    {
      kb->killsignal = KB_CMD_IGNORE;
    } else if (!parse_keybind(kb, val)) {
      errmsg = "invalid binding string";
      goto error;
    }
  } else if (strcmp(key, "path") == 0) {
    if (strcmp(val, "$CWD") == 0) val = "$PWD";
    else if (strcmp(val, "$PWD") == 0) {}
    else if (strcmp(val, "$HOME") == 0) {}
    else if (val[0] == '/') {}
    else {
      errmsg = "invalid path";
      goto error;
    }
    free(kb->path);
    kb->path = strdup(val);
    if (!kb->path) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
  } else if (strcmp(key, "color") == 0) {
    if (strEquCI(val, "bw") || strEquCI(val, "mono")) kb->colorMode = CMODE_BW;
    else if (strEquCI(val, "green")) kb->colorMode = CMODE_GREEN;
    else if (strEquCI(val, "normal")) kb->colorMode = CMODE_NORMAL;
    else if (strEquCI(val, "color")) kb->colorMode = CMODE_NORMAL;
    else if (strEquCI(val, "default")) kb->colorMode = -1;
    else {
      errmsg = "invalid color mode";
      goto error;
    }
  } else if (strcmp(key, "kill") == 0) {
    int sig = parse_signal_name(val);
    if (sig == 0) {
      errmsg = "invalid signal name";
      goto error;
    }
    free(kb->exec); kb->exec = NULL;
    if (kb->killsignal != KB_CMD_IGNORE) kb->killsignal = sig;
  } else if (strcmp(key, "exec") == 0) {
    free(kb->exec);
    kb->exec = strdup(val);
    if (!kb->exec) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
    if (kb->killsignal != KB_CMD_IGNORE) kb->killsignal = 0;
  } else if (strcmp(key, "paste") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "primary")) kb->killsignal = KB_CMD_PASTE_PRIMARY;
    else if (strEquCI(val, "secondary")) kb->killsignal = KB_CMD_PASTE_SECONDARY;
    else if (strEquCI(val, "clipboard")) kb->killsignal = KB_CMD_PASTE_CLIPBOARD;
    else {
      errmsg = "invalid selection name";
      goto error;
    }
  } else if (strcmp(key, "copy-to") == 0 || strcmp(key, "copyto") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "primary")) kb->killsignal = KB_CMD_COPY_TO_PRIMARY;
    else if (strEquCI(val, "secondary")) kb->killsignal = KB_CMD_COPY_TO_SECONDARY;
    else if (strEquCI(val, "clipboard")) kb->killsignal = KB_CMD_COPY_TO_CLIPBOARD;
    else {
      errmsg = "invalid selection name";
      goto error;
    }
  } else if (strcmp(key, "tab") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "prev")) kb->killsignal = KB_CMD_TAB_PREV;
    else if (strEquCI(val, "next")) kb->killsignal = KB_CMD_TAB_NEXT;
    else if (strEquCI(val, "first")) kb->killsignal = KB_CMD_TAB_FIRST;
    else if (strEquCI(val, "last")) kb->killsignal = KB_CMD_TAB_LAST;
    else if (strEquCI(val, "move-left")) kb->killsignal = KB_CMD_TAB_MOVE_LEFT;
    else if (strEquCI(val, "move-right")) kb->killsignal = KB_CMD_TAB_MOVE_RIGHT;
    else {
      errmsg = "invalid tab command";
      goto error;
    }
  } else if (strcmp(key, "history") == 0) {
    // for "exec"
    if (strEquCI(val, "default")) {
      kb->allow_history = -1;
    } else {
      int bval = parse_bool(val);
      if (bval < 0) {
        errmsg = "invalid history command";
        goto error;
      }
      kb->allow_history = bval;
    }
  } else if (strcmp(key, "selection") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "start")) kb->killsignal = KB_CMD_START_SELECTION;
    else {
      errmsg = "invalid selection command";
      goto error;
    }
  } else if (strcmp(key, "mouse-report") == 0 || strcmp(key, "mouse-reports") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "toggle")) kb->killsignal = KB_CMD_MREPORTS_TOGGLE;
    else {
      int bval = parse_bool(val);
      if (bval < 0) {
        errmsg = "invalid mouse-report command";
        goto error;
      }
      kb->killsignal = KB_CMD_MREPORTS_OFF + bval;
    }
  } else if (strcmp(key, "escesc") == 0 || strcmp(key, "esc-esc") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "toggle")) kb->killsignal = KB_CMD_ESCESC_TOGGLE;
    else {
      int bval = parse_bool(val);
      if (bval < 0) {
        errmsg = "invalid escesc command";
        goto error;
      }
      kb->killsignal = KB_CMD_ESCESC_OFF + bval;
    }
  } else if (strcmp(key, "cursor") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "toggle")) kb->killsignal = KB_CMD_CURSOR_TOGGLE;
    else {
      int bval = parse_bool(val);
      if (bval < 0) {
        errmsg = "invalid escesc command";
        goto error;
      }
      kb->killsignal = KB_CMD_CURSOR_OFF + bval;
    }
  } else if (strcmp(key, "history-mode") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "toggle")) kb->killsignal = KB_CMD_HISTORY_TOGGLE;
    else {
      int bval = parse_bool(val);
      if (bval < 0) {
        errmsg = "invalid history-mode command";
        goto error;
      }
      kb->killsignal = KB_CMD_HISTORY_OFF + bval;
    }
  } else if (strcmp(key, "color-mode") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "normal")) kb->killsignal = KB_CMD_COLOR_NORMAL;
    else if (strEquCI(val, "color")) kb->killsignal = KB_CMD_COLOR_NORMAL;
    else if (strEquCI(val, "default")) kb->killsignal = KB_CMD_COLOR_DEFAULT;
    else if (strEquCI(val, "bw") || strEquCI(val, "mono")) kb->killsignal = KB_CMD_COLOR_BW;
    else if (strEquCI(val, "green")) kb->killsignal = KB_CMD_COLOR_GREEN;
    else {
      errmsg = "invalid color-mode command";
      goto error;
    }
  } else if (strcmp(key, "menu") == 0) {
    free(kb->exec); kb->exec = NULL;
    if (strEquCI(val, "tab-options")) kb->killsignal = KB_CMD_MENU_TAB_OPTIONS;
    else if (strEquCI(val, "global-options")) kb->killsignal = KB_CMD_MENU_OPTIONS;
    else {
      errmsg = "invalid menu command";
      goto error;
    }
  } else {
    errmsg = "unknown keybind option";
    goto error;
  }

  return False;

error:
  if (xrm_reloading) {
    show_error_menu(errmsg, quarks);
  }

  fprintf(stderr, "CONFIG ERROR(1): %s!\n", errmsg);
  fprintf(stderr, "         key: ");
  int f = 0;
  while (quarks[f] != NULLQUARK) {
    const char *qq = XrmQuarkToString(quarks[f]);
    if (f != 0) fputc('.', stderr);
    fprintf(stderr, "%s", qq);
    f += 1;
  }
  fputc('\n', stderr);

  if (type) {
    fprintf(stderr, "        type: %s\n", XrmQuarkToString(*type));
    if (strcmp(XrmQuarkToString(*type), "String") == 0) {
      fprintf(stderr, "       value: <%.*s>\n", (unsigned)value->size, (const char *)value->addr);
    }
  }

  if (!xrm_reloading) exit(1);
  return False;
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  XRM_BOOL,
  XRM_MSTIME, // positive
  XRM_TABCOUNT,
  XRM_FPS,
  XRM_FONT,
  XRM_FONTGFX,
  XRM_COLOR,
  XRM_COLOR_OR_NONE,
  XRM_SHADOW_OFS,
  XRM_STRING, // never empty
  XRM_TERMTYPE,
};

typedef struct {
  const char *key;
  int type;
  yterm_bool once; // load only once?
  yterm_bool sealed;
  void *varptr;
  XrmQuarkList quarks; // will be sorted
} IniKey;


static IniKey inikeys[] = {
  {.key="window.title", .type=XRM_STRING, .varptr=&opt_title, .once=1},

  {.key="term.name", .type=XRM_STRING,   .varptr=&opt_term, .once=1},
  {.key="term.type", .type=XRM_TERMTYPE, .varptr=&opt_term_type, .once=1},
  {.key="term.font", .type=XRM_FONT,     .varptr=&opt_mono_font, .once=1},

  {.key="term.font.gfx",    .type=XRM_FONTGFX, .varptr=&opt_terminus_gfx, .once=1},
  {.key="term.winch.delay", .type=XRM_MSTIME,  .varptr=&opt_winch_delay},
  {.key="term.exec.delay",  .type=XRM_MSTIME,  .varptr=&opt_exec_delay},

  {.key="tabs.font",    .type=XRM_FONT,     .varptr=&opt_tabs_font, .once=1},
  {.key="tabs.visible", .type=XRM_TABCOUNT, .varptr=&opt_tabs_visible, .once=1},
  {.key="tabs.cyclic",  .type=XRM_BOOL,     .varptr=&opt_cycle_tabs},

  {.key="paste.primary",   .type=XRM_STRING, .varptr=&opt_paste_from[0]},
  {.key="paste.secondary", .type=XRM_STRING, .varptr=&opt_paste_from[1]},
  {.key="paste.clipboard", .type=XRM_STRING, .varptr=&opt_paste_from[2]},

  {.key="copyto.primary",   .type=XRM_STRING, .varptr=&opt_copy_to[0]},
  {.key="copyto.secondary", .type=XRM_STRING, .varptr=&opt_copy_to[1]},
  {.key="copyto.clipboard", .type=XRM_STRING, .varptr=&opt_copy_to[2]},

  {.key="refresh.fps", .type=XRM_FPS, .varptr=&opt_fps},

  {.key="history.enabled", .type=XRM_BOOL, .varptr=&opt_history_enabled},

  {.key="esc-esc.fix",   .type=XRM_BOOL, .varptr=&opt_esc_twice},
  {.key="mouse.reports", .type=XRM_BOOL, .varptr=&opt_mouse_reports},

  {.key="idiotic.vim", .type=XRM_BOOL, .varptr=&opt_workaround_vim_idiocity},

  {.key="cursor.blink.time",     .type=XRM_MSTIME,        .varptr=&opt_cur_blink_time},
  {.key="cursor.color0.fg",      .type=XRM_COLOR_OR_NONE, .varptr=&curColorsFG[0]},
  {.key="cursor.color0.bg",      .type=XRM_COLOR_OR_NONE, .varptr=&curColorsBG[0]},
  {.key="cursor.color1.fg",      .type=XRM_COLOR_OR_NONE, .varptr=&curColorsFG[1]},
  {.key="cursor.color1.bg",      .type=XRM_COLOR_OR_NONE, .varptr=&curColorsBG[1]},
  {.key="cursor.color.inactive", .type=XRM_COLOR_OR_NONE, .varptr=&curColorsBG[2]},

  {.key="color.default.fg",      .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_DEFAULT_FG]},
  {.key="color.default.bg",      .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_DEFAULT_BG]},
  {.key="color.default.fg.high", .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_DEFAULT_HIGH_FG]},
  {.key="color.default.bg.high", .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_DEFAULT_HIGH_BG]},

  {.key="color.bold.fg",           .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_BOLD_FG]},
  {.key="color.underline.fg",      .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_UNDER_FG]},
  {.key="color.bold.underline.fg", .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_BOLD_UNDER_FG]},

  {.key="color.high.bold.fg",           .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_BOLD_HIGH_FG]},
  {.key="color.high.underline.fg",      .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_UNDER_HIGH_FG]},
  {.key="color.high.bold.underline.fg", .type=XRM_COLOR, .varptr=&colorsTTY[CIDX_BOLD_UNDER_HIGH_FG]},

  {.key="tab.color.frame.fg", .type=XRM_COLOR, .varptr=&tab_frame_fg},
  {.key="tab.color.frame.bg", .type=XRM_COLOR, .varptr=&tab_frame_bg},

  {.key="tab.color.normal.bg", .type=XRM_COLOR, .varptr=&tab_colors[TABC_NORMAL].bg},
  {.key="tab.color.normal.text", .type=XRM_COLOR, .varptr=&tab_colors[TABC_NORMAL].text},
  {.key="tab.color.normal.index", .type=XRM_COLOR, .varptr=&tab_colors[TABC_NORMAL].index},
  {.key="tab.color.normal.ellipsis", .type=XRM_COLOR, .varptr=&tab_colors[TABC_NORMAL].ellipsis},

  {.key="tab.color.active.bg", .type=XRM_COLOR, .varptr=&tab_colors[TABC_ACTIVE].bg},
  {.key="tab.color.active.text", .type=XRM_COLOR, .varptr=&tab_colors[TABC_ACTIVE].text},
  {.key="tab.color.active.index", .type=XRM_COLOR, .varptr=&tab_colors[TABC_ACTIVE].index},
  {.key="tab.color.active.ellipsis", .type=XRM_COLOR, .varptr=&tab_colors[TABC_ACTIVE].ellipsis},

  {.key="tab.color.dead.bg", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD].bg},
  {.key="tab.color.dead.text", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD].text},
  {.key="tab.color.dead.index", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD].index},
  {.key="tab.color.dead.ellipsis", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD].ellipsis},

  {.key="tab.color.dead.active.bg", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD_ACTIVE].bg},
  {.key="tab.color.dead.active.text", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD_ACTIVE].text},
  {.key="tab.color.dead.active.index", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD_ACTIVE].index},
  {.key="tab.color.dead.active.ellipsis", .type=XRM_COLOR, .varptr=&tab_colors[TABC_DEAD_ACTIVE].ellipsis},

  {.key="selection.color.fg", .type=XRM_COLOR, .varptr=&colorAmberFG},
  {.key="selection.color.bg", .type=XRM_COLOR, .varptr=&colorAmberBG},
  {.key="selection.block.color.fg", .type=XRM_COLOR, .varptr=&colorSelectionFG},
  {.key="selection.block.color.bg", .type=XRM_COLOR, .varptr=&colorSelectionBG},

  {.key="selection.tint", .type=XRM_BOOL, .varptr=&opt_amber_tint},

  {.key="osd-menu.shadow.color", .type=XRM_COLOR_OR_NONE, .varptr=&opt_osd_menu_shadow_color},
  {.key="osd-menu.shadow.x-ofs", .type=XRM_SHADOW_OFS, .varptr=&opt_osd_menu_shadow_xofs},
  {.key="osd-menu.shadow.y-ofs", .type=XRM_SHADOW_OFS, .varptr=&opt_osd_menu_shadow_yofs},
  {.key="osd-menu.shadow.xofs", .type=XRM_SHADOW_OFS, .varptr=&opt_osd_menu_shadow_xofs},
  {.key="osd-menu.shadow.yofs", .type=XRM_SHADOW_OFS, .varptr=&opt_osd_menu_shadow_yofs},

  {.key="osd-menu.color.passive.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[0]},
  {.key="osd-menu.color.passive.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[0]},

  {.key="osd-menu.color.active.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[OSD_CSCHEME_ACTIVE]},
  {.key="osd-menu.color.active.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[OSD_CSCHEME_ACTIVE]},

  {.key="osd-menu.color.passive.cursor.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[OSD_CSCHEME_CURSOR]},
  {.key="osd-menu.color.passive.cursor.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[OSD_CSCHEME_CURSOR]},

  {.key="osd-menu.color.active.cursor.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[OSD_CSCHEME_CURSOR | OSD_CSCHEME_ACTIVE]},
  {.key="osd-menu.color.active.cursor.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[OSD_CSCHEME_CURSOR | OSD_CSCHEME_ACTIVE]},

  {.key="osd-menu.color.passive.hotkey.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[OSD_CSCHEME_HOTKEY]},
  {.key="osd-menu.color.passive.hotkey.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[OSD_CSCHEME_HOTKEY]},

  {.key="osd-menu.color.active.hotkey.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[OSD_CSCHEME_HOTKEY | OSD_CSCHEME_ACTIVE]},
  {.key="osd-menu.color.active.hotkey.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[OSD_CSCHEME_HOTKEY | OSD_CSCHEME_ACTIVE]},

  {.key="osd-menu.color.passive.cursor.hotkey.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[OSD_CSCHEME_CURSOR | OSD_CSCHEME_HOTKEY]},
  {.key="osd-menu.color.passive.cursor.hotkey.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[OSD_CSCHEME_CURSOR | OSD_CSCHEME_HOTKEY]},

  {.key="osd-menu.color.active.cursor.hotkey.bg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_bg[OSD_CSCHEME_CURSOR | OSD_CSCHEME_HOTKEY | OSD_CSCHEME_ACTIVE]},
  {.key="osd-menu.color.active.cursor.hotkey.fg", .type=XRM_COLOR_OR_NONE, .varptr=&osd_menu_fg[OSD_CSCHEME_CURSOR | OSD_CSCHEME_HOTKEY | OSD_CSCHEME_ACTIVE]},

  {.key=NULL},
};


//==========================================================================
//
//  xq_cmp_cb
//
//==========================================================================
static int xq_cmp_cb (const void *aa, const void *bb) {
  XrmQuark a = *(const XrmQuark *)aa;
  XrmQuark b = *(const XrmQuark *)bb;
  return (a < b ? -1 : a > b ? +1 : 0);
}


//==========================================================================
//
//  prepare_ikey_name
//
//==========================================================================
static void prepare_ikey_name (IniKey *ikey) {
  yterm_assert(ikey != NULL);
  if (ikey->quarks == NULL) {
    // parse key name
    char ktemp[128];
    yterm_assert(strlen(ikey->key) < sizeof(ktemp));
    strcpy(ktemp, ikey->key);
    int pqcount = 2;
    for (const char *tmp = ikey->key; *tmp; tmp += 1) {
      if (*tmp == '.') pqcount += 1;
    }
    ikey->quarks = malloc((unsigned)pqcount * sizeof(ikey->quarks[0]));
    pqcount = 0;
    const char *kstr = ikey->key;
    while (*kstr) {
      if (*kstr == '.') {
        kstr += 1;
      } else {
        const char *ep = strchr(kstr, '.');
        if (ep == NULL) ep = kstr + strlen(kstr);
        const size_t klen = (size_t)(ptrdiff_t)(ep - kstr);
        yterm_assert(klen != 0);
        if (klen >= sizeof(ktemp)) abort();
        memcpy(ktemp, kstr, klen);
        ktemp[klen] = 0;
        kstr = ep;
        ikey->quarks[pqcount] = XrmStringToQuark(ktemp);
        pqcount += 1;
      }
    }
    yterm_assert(pqcount != 0);
    qsort(&ikey->quarks[0], (size_t)pqcount, sizeof(ikey->quarks[0]), xq_cmp_cb);
    ikey->quarks[pqcount] = NULLQUARK;
    #if 0
    fprintf(stderr, "PARSED: ");
    for (int cc = 0; ikey->quarks[cc]; cc += 1) {
      if (cc != 0) fputc('.', stderr);
      fprintf(stderr, "%s", XrmQuarkToString(ikey->quarks[cc]));
    }
    fputc('\n', stderr);
    #endif
  }
}


#define DUMP_KEY_NAME()  do { \
  int qnn = 0; \
  while (quarks[qnn] != NULLQUARK) { \
    if (qnn != 0) fputc('.', stderr); \
    fprintf(stderr, "%s", XrmQuarkToString(quarks[qnn])); \
    qnn += 1; \
  } \
} while (0)


#define DUMP_SKKEY_NAME()  do { \
  int qnn = 0; \
  while (qlist[qnn] != NULLQUARK) { \
    if (qnn != 0) fputc('.', stderr); \
    fprintf(stderr, "%s", XrmQuarkToString(qlist[qnn])); \
    qnn += 1; \
  } \
} while (0)


//==========================================================================
//
//  xrm_load_inis_cb
//
//==========================================================================
static Bool xrm_load_inis_cb (XrmDatabase *db, XrmBindingList bindings, XrmQuarkList quarks,
                              XrmRepresentation *type, XrmValue *value, XPointer udata)
{
  (void)db; (void)bindings; (void)quarks; (void)type; (void)value; (void)udata;
  #if 0
  fprintf(stderr, "=====\n");
  #endif

  const char *errmsg = NULL;

  if (quarks[0] == NULLQUARK || quarks[1] == NULLQUARK) {
    //errmsg = "invalid key name";
    //goto error;
    return False;
  }

  XrmQuark qlist[16];
  int count = 0;
  while (count < 16 && quarks[count + 1] != NULLQUARK) {
    // keybind?
    if (strcmp(XrmQuarkToString(quarks[count + 1]), "keybind") == 0) {
      return xrm_keybind_cb(db, bindings, quarks, type, value, udata);
    }
    qlist[count] = quarks[count + 1];
    count += 1;
  }

  if (count < 1 || count > 15) return False;
  qlist[count] = NULLQUARK;

  qsort(&qlist[0], (size_t)count, sizeof(qlist[0]), xq_cmp_cb);

  #if 0
  fprintf(stderr, "***: "); DUMP_SKKEY_NAME(); fputc('\n', stderr);
  #endif

  for (IniKey *ikey = inikeys; ikey->key != NULL; ikey += 1) {
    prepare_ikey_name(ikey);
    // compare
    int pos = 0;
    while (qlist[pos] != NULLQUARK && ikey->quarks[pos] != NULLQUARK &&
           qlist[pos] == ikey->quarks[pos])
    {
      pos += 1;
    }
    if (qlist[pos] == NULLQUARK && ikey->quarks[pos] == NULLQUARK) {
      if (ikey->sealed) return False; // this option is sealed
      if (type == NULL || strcmp(XrmQuarkToString(*type), "String") != 0) {
        errmsg = "invalid key type";
        goto error;
      }
      char *sval = xrm_val_to_val(value);
      if (sval == NULL) {
        errmsg = "invalid key value";
        goto error;
      }
      switch (ikey->type) {
        case XRM_BOOL:
          {
            int bval = parse_bool(sval);
            if (bval < 0) {
              errmsg = "invalid boolean value";
              goto error;
            }
            *(yterm_bool *)ikey->varptr = bval;
          }
          break;
        case XRM_MSTIME: // positive
          {
            int bval = parse_bool(sval);
            if (bval == 0) {
              *(int *)ikey->varptr = 0;
            } else {
              char *end;
              long v = strtol(sval, &end, 10);
              if (*end || v < 0 || v > 2666) {
                errmsg = "invalid time value";
                goto error;
              }
              *(int *)ikey->varptr = (int)v;
            }
          }
          break;
        case XRM_FPS:
          {
            char *end;
            long v = strtol(sval, &end, 10);
            if (*end || v < 1 || v > 120) {
              errmsg = "invalid FPS value";
              goto error;
            }
            *(int *)ikey->varptr = (int)v;
          }
          break;
        case XRM_SHADOW_OFS:
          {
            char *end;
            long v = strtol(sval, &end, 10);
            if (*end || v < 1 || v > 64) {
              errmsg = "invalid shadow offset value";
              goto error;
            }
            *(int *)ikey->varptr = (int)v;
          }
          break;
        case XRM_TABCOUNT:
          {
            char *end;
            long v = strtol(sval, &end, 10);
            if (*end || v < 0 || v > 32) {
              int bval = parse_bool(sval);
              if (bval < 0) {
                errmsg = "invalid tab count value";
                goto error;
              }
              opt_enable_tabs = bval;
            } else {
              if (v == 0) {
                opt_enable_tabs = 0;
              } else {
                *(int *)ikey->varptr = (int)v;
              }
            }
          }
          break;
        case XRM_FONT:
          {
            char **sptr = (char **)ikey->varptr;
            free(*sptr);
            *sptr = strdup(sval);
            if (*sptr == NULL) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
          }
          break;
        case XRM_COLOR:
          {
            uint32_t clr = parse_hex_color(sval);
            if (clr == XRM_BAD_COLOR) {
              errmsg = "invalid color value";
              goto error;
            }
            *(uint32_t *)ikey->varptr = clr;
          }
          break;
        case XRM_COLOR_OR_NONE:
          {
            if (strEquCI(sval, "none")) {
              *(uint32_t *)ikey->varptr = 0xff000000U;
            } else {
              uint32_t clr = parse_hex_color(sval);
              if (clr == XRM_BAD_COLOR) {
                errmsg = "invalid color value";
                goto error;
              }
              *(uint32_t *)ikey->varptr = clr;
            }
          }
          break;
        case XRM_STRING: // never empty
          {
            char **sptr = (char **)ikey->varptr;
            free(*sptr);
            *sptr = strdup(sval);
            if (*sptr == NULL) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
          }
          break;
        case XRM_TERMTYPE:
          if (strEquCI(sval, "rxvt")) opt_term_type = TT_RXVT;
          else if (strEquCI(sval, "xterm")) opt_term_type = TT_XTERM;
          else {
            errmsg = "invalid terminal type";
            goto error;
          }
          break;
        case XRM_FONTGFX:
          if (strEquCI(sval, "terminus")) opt_terminus_gfx = 1;
          else if (strEquCI(sval, "unicode")) opt_terminus_gfx = 0;
          else {
            errmsg = "invalid font gfx type";
            goto error;
          }
          break;
        default:
          fprintf(stderr, "INTERNAL ERROR: ketmar forgot to handle some ini type!\n");
          abort();
      }
      #if 0
      fprintf(stderr, "OK KEY: "); DUMP_KEY_NAME(); fputc('\n', stderr);
      fprintf(stderr, "  type: %d; val=<%s>\n", ikey->type, sval);
      #endif
      return False;
    }
  }

  // terminal colors?
  if (count == 1) {
    const char *qn = XrmQuarkToString(qlist[0]);
    int idx = -1;
    for (int f = 0; idx == -1 && f < 16; f += 1) {
      char kname[16];
      snprintf(kname, sizeof(kname), "color%d", f);
      if (strcmp(qn, kname) == 0) idx = f;
      else if (f < 10) {
        snprintf(kname, sizeof(kname), "color0%d", f);
        if (strcmp(qn, kname) == 0) idx = f;
      }
    }
    if (idx != -1) {
      yterm_assert(idx >= 0 && idx <= 15);
      #if 0
      fprintf(stderr, "TTYCOLOR %d KEY: ", idx); DUMP_KEY_NAME(); fputc('\n', stderr);
      #endif
      char *sval = xrm_val_to_val(value);
      if (sval == NULL) {
        errmsg = "invalid key value";
        goto error;
      }
      uint32_t clr = parse_hex_color(sval);
      if (clr == XRM_BAD_COLOR) {
        errmsg = "invalid color value";
        goto error;
      }
      colorsTTY[idx] = clr;
      return False;
    }
  }

  fprintf(stderr, "UNKNOWN KEY: "); DUMP_KEY_NAME(); fputc('\n', stderr);

  return False;

error:
  if (xrm_reloading) {
    show_error_menu(errmsg, quarks);
  }

  fprintf(stderr, "CONFIG ERROR(2): %s!\n", errmsg);
  fprintf(stderr, "         key: ");
  int f = 0;
  while (quarks[f] != NULLQUARK) {
    const char *qq = XrmQuarkToString(quarks[f]);
    if (f != 0) fputc('.', stderr);
    fprintf(stderr, "%s", qq);
    f += 1;
  }
  fputc('\n', stderr);

  if (type) {
    fprintf(stderr, "        type: %s\n", XrmQuarkToString(*type));
    if (strcmp(XrmQuarkToString(*type), "String") == 0) {
      fprintf(stderr, "       value: <%.*s>\n", (unsigned)value->size, (const char *)value->addr);
    }
  }

  if (!xrm_reloading) exit(1);
  return False;
}


//==========================================================================
//
//  xrm_load_inis
//
//==========================================================================
static void xrm_load_inis (XrmDatabase db) {
  XrmQuark names[2];
  XrmQuark classes[2];
  //
  names[0] = XrmPermStringToQuark(xrm_app_name);
  names[1] = NULLQUARK;
  // class
  classes[0] = NULLQUARK;

  XrmEnumerateDatabase(db, names, classes, XrmEnumAllLevels, xrm_load_inis_cb, NULL);
}


//==========================================================================
//
//  xrm_seal_option_by_ptr
//
//==========================================================================
static void xrm_seal_option_by_ptr (const void *ptr) {
  for (IniKey *ikey = inikeys; ikey->key != NULL; ikey += 1) {
    if (ikey->varptr == ptr && ikey->sealed == 0) {
      ikey->sealed = -1;
    }
  }
}


//==========================================================================
//
//  xrm_is_option_sealed_by_ptr
//
//==========================================================================
static yterm_bool xrm_is_option_sealed_by_ptr (const void *ptr) {
  yterm_bool res = 0;
  for (IniKey *ikey = inikeys; res == 0 && ikey->key != NULL; ikey += 1) {
    res = (ikey->varptr == ptr && ikey->sealed == -1);
  }
  return res;
}


//==========================================================================
//
//  xrm_unseal_ptr_options
//
//==========================================================================
static void xrm_unseal_ptr_options (void) {
  for (IniKey *ikey = inikeys; ikey->key != NULL; ikey += 1) {
    if (ikey->sealed == -1) ikey->sealed = 0;
  }
}


//==========================================================================
//
//  xrm_new_command_keybind
//
//==========================================================================
static void xrm_new_command_keybind (const char *name, const char *keys, int cmd) {
  yterm_assert(name != NULL);
  yterm_assert(keys != NULL);
  KeyBind *kb = malloc(sizeof(KeyBind));
  if (!kb) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
  memset(kb, 0, sizeof(KeyBind));
  kb->bindname = strdup(name);
  if (!kb->bindname) abort();
  if (!parse_keybind(kb, keys)) abort();
  kb->colorMode = -1;
  kb->killsignal = cmd;
  kb->allow_history = -1;
  kb->next = keybinds;
  keybinds = kb;
}


//==========================================================================
//
//  xrm_new_exec_keybind
//
//==========================================================================
static void xrm_new_exec_keybind (const char *name, const char *keys,
                                  const char *exec, const char *path)
{
  yterm_assert(name != NULL);
  yterm_assert(keys != NULL);
  yterm_assert(exec != NULL);
  yterm_assert(path != NULL);
  KeyBind *kb = malloc(sizeof(KeyBind));
  if (!kb) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
  memset(kb, 0, sizeof(KeyBind));
  kb->bindname = strdup(name);
  if (!kb->bindname) abort();
  if (!parse_keybind(kb, keys)) abort();
  kb->colorMode = -1;
  kb->killsignal = 0;
  kb->allow_history = -1;
  kb->exec = strdup(exec);
  if (!kb->exec) abort();
  kb->path = strdup(path);
  if (!kb->path) abort();
  kb->next = keybinds;
  keybinds = kb;
}


//==========================================================================
//
//  xrm_add_default_keybinds
//
//==========================================================================
static void xrm_add_default_keybinds (void) {
  fprintf(stderr, "adding default keybindings...\n");

  xrm_new_command_keybind("start-selection", "C-space", KB_CMD_START_SELECTION);
  xrm_new_command_keybind("start-selection1", "C-KP_Insert", KB_CMD_START_SELECTION);
  xrm_new_command_keybind("start-selection2", "M-KP_Insert", KB_CMD_START_SELECTION);

  xrm_new_command_keybind("paste-primary", "S-Insert", KB_CMD_PASTE_PRIMARY);
  xrm_new_command_keybind("paste-secondary", "M-Insert", KB_CMD_PASTE_SECONDARY);
  xrm_new_command_keybind("paste-clipboard", "M-S-Insert", KB_CMD_PASTE_CLIPBOARD);

  xrm_new_command_keybind("copy-to-primary", "S-Insert", KB_CMD_COPY_TO_PRIMARY);
  xrm_new_command_keybind("copy-to-secondary", "M-Insert", KB_CMD_COPY_TO_SECONDARY);
  xrm_new_command_keybind("copy-to-clipboard", "M-S-Insert", KB_CMD_COPY_TO_CLIPBOARD);
  xrm_new_command_keybind("copy-to-primary", "M-S-c", KB_CMD_COPY_TO_PRIMARY);

  xrm_new_command_keybind("prev-tab", "C-M-Left", KB_CMD_TAB_PREV);
  xrm_new_command_keybind("next-tab", "C-M-Right", KB_CMD_TAB_NEXT);
  xrm_new_command_keybind("first-tab", "C-M-Home", KB_CMD_TAB_FIRST);
  xrm_new_command_keybind("last-tab", "C-M-End", KB_CMD_TAB_LAST);
  xrm_new_command_keybind("tab-to-left", "M-S-Left", KB_CMD_TAB_MOVE_LEFT);
  xrm_new_command_keybind("tab-to-right", "M-S-Right", KB_CMD_TAB_MOVE_RIGHT);

  xrm_new_command_keybind("kill-process", "C-H-k", SIGKILL);

  xrm_new_exec_keybind("newtab", "C-M-t", "$SHELL", "$PWD");
  xrm_new_exec_keybind("root-shell", "C-M-r", "suka -F -", "$PWD");
  keybinds->colorMode = CMODE_GREEN;
  xrm_new_exec_keybind("mc", "C-M-m", "mc", "$PWD");

  xrm_new_command_keybind("menu-tab-options", "Menu", KB_CMD_MENU_TAB_OPTIONS);
}


//==========================================================================
//
//  xrm_load_options
//
//==========================================================================
static void xrm_load_options (void) {
  XrmInitialize();

  // some idiotic X11 instances doesn't have a system-wide resources
  // WARNING! don't free `sysdbstr()`! that's how it works.
  char *sysdbstr = XResourceManagerString(x11_dpy);
  XrmDatabase db = XrmGetStringDatabase(sysdbstr != NULL ? sysdbstr : "");

  // clear keybinds
  while (keybinds != NULL) {
    KeyBind *kb = keybinds;
    keybinds = keybinds->next;
    free(kb->bindname);
    free(kb->exec);
    free(kb->path);
  }

  // load local settings
  {
    const char *home = getenv("HOME");
    if (home && home[0]) {
      char *fname = malloc(strlen(home) + 128);
      strcpy(fname, home);
      if (home[strlen(home) - 1] != '/') strcat(fname, "/");
      strcat(fname, ".k8-yterm.Xresources");
      //fprintf(stderr, "<%s>\n", fname);
      if (db) {
        if (XrmCombineFileDatabase(fname, &db, True) == 0) {
          //fprintf(stderr, "SHIT!\n");
        }
      } else {
        db = XrmGetFileDatabase(fname);
      }
      free(fname);
    }
  }

  if (db) {
    //xrm_load_keybinds(db);
    xrm_load_inis(db);

    for (IniKey *ikey = inikeys; ikey->key != NULL; ikey += 1) {
      free(ikey->quarks);
      ikey->quarks = NULL;
    }

    XrmDestroyDatabase(db);

    yterm_assert(opt_tabs_visible > 0);
    // check if we have more than one tab open, and don't disable tabs in this case
    if (!opt_enable_tabs) {
      Term *tt = termlist;
      while (tt != NULL && tt->deadstate == DS_DEAD) tt = tt->next;
      if (tt != NULL) opt_enable_tabs = 1;
    }

    // seal options
    for (IniKey *ikey = inikeys; ikey->key != NULL; ikey += 1) {
      if (ikey->once) ikey->sealed = 1;
    }

    // remove dead keybinds
    KeyBind *prev = NULL;
    KeyBind *cc = NULL;
    KeyBind *kb = keybinds;
    while (kb != NULL) {
      if (kb->ksym == NoSymbol || kb->killsignal == KB_CMD_IGNORE ||
          (kb->killsignal <= 0 && kb->exec == NULL))
      {
        fprintf(stderr, "XRM: removed empty/disabled keybind '%s'\n", kb->bindname);
        free(kb->bindname);
        free(kb->exec);
        free(kb->path);
        if (prev != NULL) prev->next = kb->next; else keybinds = kb->next;
        cc = kb;
        kb = kb->next;
        free(cc);
      } else {
        prev = kb; kb = kb->next;
      }
    }

    // if we have no keybinds, add default
    if (keybinds == NULL) xrm_add_default_keybinds();

    // check for keybind conflicts
    for (kb = keybinds; kb != NULL; kb = kb->next) {
      for (KeyBind *nkb = kb->next; nkb != NULL; nkb = nkb->next) {
        if (nkb != kb && nkb->ksym == kb->ksym && nkb->mods == kb->mods) {
          yterm_bool bad = 1;
          if (kb->killsignal == KB_CMD_PASTE_PRIMARY ||
              kb->killsignal == KB_CMD_PASTE_SECONDARY ||
              kb->killsignal == KB_CMD_PASTE_CLIPBOARD)
          {
            bad = nkb->killsignal != KB_CMD_COPY_TO_PRIMARY &&
                  nkb->killsignal != KB_CMD_COPY_TO_SECONDARY &&
                  nkb->killsignal != KB_CMD_COPY_TO_CLIPBOARD;
          } else if (kb->killsignal == KB_CMD_COPY_TO_PRIMARY ||
                     kb->killsignal == KB_CMD_COPY_TO_SECONDARY ||
                     kb->killsignal == KB_CMD_COPY_TO_CLIPBOARD)
          {
            bad = nkb->killsignal != KB_CMD_PASTE_PRIMARY &&
                  nkb->killsignal != KB_CMD_PASTE_SECONDARY &&
                  nkb->killsignal != KB_CMD_PASTE_CLIPBOARD;
          }
          if (bad) {
            fprintf(stderr, "WARNING! keybind '%s' conflicts with '%s'!\n",
                    kb->bindname, nkb->bindname);
          }
        }
      }
    }

    #if 0
    // dump keybinds
    if (keybinds != NULL) {
      fprintf(stderr, "=== KEYBINDS ===\n");
      for (kb = keybinds; kb != NULL; kb = kb->next) {
        const char *ksname = XKeysymToString(kb->ksym);
        fprintf(stderr, "--- %s ---\n", kb->bindname);
        fprintf(stderr, " key: ");
        if (kb->mods & ControlMask) fprintf(stderr, "C-");
        if (kb->mods & Mod1Mask) fprintf(stderr, "M-");
        if (kb->mods & Mod4Mask) fprintf(stderr, "H-");
        if (kb->mods & ShiftMask) fprintf(stderr, "S-");
        fprintf(stderr, "%s\n", ksname);
        fprintf(stderr, " path: %s\n", (kb->path != NULL ? kb->path : "<default>"));
        fprintf(stderr, " exec: %s\n", (kb->exec != NULL ? kb->exec : "<none>"));
        if (kb->killsignal >= 600) {
          fprintf(stderr, " command: %d\n", kb->killsignal);
        } else if (kb->killsignal > 0) {
          fprintf(stderr, " kill: %d\n", kb->killsignal);
        } else {
          fprintf(stderr, " kill: <none>\n");
        }
      }
    }
    #endif

  } else {
    fprintf(stderr, "NOTE: cannot load X resources database (meh).\n");
  }
}
