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
  X11 initialisation and stuff.
  included directly into the main file.
*/


#if FORCE_LOCALE
//==========================================================================
//
//  isUTF8LocaleName
//
//==========================================================================
static yterm_bool isUTF8LocaleName (const char *lang) {
  if (!lang) return 0;
  while (*lang) {
    if ((lang[0]|0x20) == 'u' && (lang[1]|0x20) == 't' && (lang[2]|0x20) == 'f') return 1;
    ++lang;
  }
  return 0;
}
#endif


//==========================================================================
//
//  isKOILocale
//
//==========================================================================
static yterm_bool isKOILocale (void) {
  int res = 0;
  const char *lc = setlocale(LC_ALL, "");
  if (lc != NULL) {
    while (!res && *lc) {
      if ((lc[0] == 'k' || lc[0] == 'K') &&
          (lc[1] == 'o' || lc[1] == 'O') &&
          (lc[2] == 'i' || lc[2] == 'I'))
      {
        res = 1;
      }
      lc += 1;
    }
  }
  return res;
}


//==========================================================================
//
//  x11_change_title
//
//==========================================================================
static void x11_change_title (const char *s) {
  if (!s || !s[0]) s = opt_title;
  size_t len = strlen(s);
  if (len > sizeof(x11_lasttitle)-1) len = sizeof(x11_lasttitle) - 1;
  const size_t tlen = strlen(x11_lasttitle);
  if (tlen == len && (len == 0 || memcmp(x11_lasttitle, s, len) == 0)) return;
  memcpy(x11_lasttitle, s, len);
  x11_lasttitle[len] = 0;
  XStoreName(x11_dpy, x11_win, x11_lasttitle);
  XChangeProperty(x11_dpy, x11_win, xaNetWMName, xaUTF8, 8, PropModeReplace,
                  (uint8_t*)x11_lasttitle, (uint32_t)strlen(x11_lasttitle));
}


//==========================================================================
//
//  x11_xgetfontinfo
//
//==========================================================================
static yterm_bool x11_xgetfontinfo (XFont *font, yterm_bool needmono) {
  int dir, ascent, descent;
  XCharStruct cs;

  XQueryTextExtents(x11_dpy, font->fst->fid, "W", 1, &dir, &ascent, &descent, &cs);
  font->ascent = ascent;
  font->descent = descent;
  font->lbearing = cs.lbearing;
  font->rbearing = cs.rbearing;
  font->width = cs.width;
  if (!(cs.width > 0 && ascent + descent > 0)) {
    fprintf(stderr, "X11: specified X font has invalid dimensions.\n");
    return 0;
  }
  //fprintf(stderr, "ASCENT: %d; DESCENT: %d; lb=%d; rb=%d\n", font->ascent, font->descent, font->lbearing, font->rbearing);

  if (needmono) {
    // rough check for monospaced font
    const char *wcs[9] = {"A", "W", "I", "i", "0", "1", "-", "g", "b"};
    int wdt[9] = {0};

    for (unsigned f = 0; f < 9; ++f) {
      wdt[f] = XTextWidth(font->fst, wcs[f], 1);
    }

    for (unsigned f = 0; f < 9; ++f) {
      for (unsigned c = 0; c < 9; ++c) {
        if (wdt[f] != wdt[c]) {
          fprintf(stderr, "X11: specified X font is not monospaced.\n");
          return 0;
        }
      }
    }
  }

  return 1;
}


//==========================================================================
//
//  x11_initfont
//
//==========================================================================
static yterm_bool x11_initfont (XFont *xfnt, const char *nfontstr, yterm_bool needmono) {
  xfnt->fst = NULL;
  if (!nfontstr || !nfontstr[0]) return 0;
  int fcount = 0;
  char **flist = XListFonts(x11_dpy, (char *)nfontstr, 32700/*maxnames*/, &fcount);
  if (flist) {
    //for (int f = 0; f < fcount; ++f) fprintf(stderr, "F#%d: <%s>\n", f, flist[f]);
    for (int f = 0; f < fcount; ++f) {
      if (strstr(flist[f], "-iso10646-1")) {
        xfnt->fst = XLoadQueryFont(x11_dpy, flist[f]);
        if (xfnt->fst) {
          int dir, ascent, descent;
          XCharStruct cs;
          XQueryTextExtents(x11_dpy, xfnt->fst->fid, "W", 1, &dir, &ascent, &descent, &cs);
          if (ascent+descent > 0 && cs.rbearing-cs.lbearing > 0 && cs.width > 0) {
            fprintf(stderr, "X11: loaded font '%s' (hgt=%d; wdt=%d)\n", flist[f], (int)(ascent+descent), (int)cs.width);
            break;
          } else {
            fprintf(stderr, "X11: rejected font '%s'\n", flist[f]);
            XFreeFont(x11_dpy, xfnt->fst);
          }
        }
      }
    }
    XFreeFontNames(flist);
  }
  if (!xfnt->fst) {
    xfnt->fst = XLoadQueryFont(x11_dpy, nfontstr);
    if (!xfnt->fst) return 0;
  }

  if (!x11_xgetfontinfo(xfnt, needmono)) {
    XFreeFont(x11_dpy, xfnt->fst);
    xfnt->fst = NULL;
    return 0;
  }

  return 1;
}


//==========================================================================
//
//  x11_init_display
//
//==========================================================================
static void x11_init_display (void) {
  yterm_assert(!x11_dpy);

  x11_lasttitle[0] = 0;

  (void)yterm_get_msecs(); // initialize

  // locale could be either KOI, or UTF. deal with it.
  koiLocale = isKOILocale();
  #if 1
  if (!koiLocale) fprintf(stderr, "DBG: utshit locale detected!\n");
  #endif

  #if FORCE_LOCALE
  // this is so all input will be in utf-8
  char *olocale = setlocale(LC_ALL, NULL);
  if (!isUTF8LocaleName(olocale)) {
    olocale = strdup(olocale);
    // force utf-8 locale
    if (!setlocale(LC_ALL, "en_US.UTF-8") &&
        !setlocale(LC_ALL, "en_US.UTF8") &&
        !setlocale(LC_ALL, "en_US.utf8"))
    {
      fprintf(stderr, "FATAL: can't set UTF locale!\n");
      exit(1);
    }
  } else {
    olocale = NULL;
  }
  #endif

  if (x11_haslocale >= 0) {
    if (!XSupportsLocale()) {
      x11_haslocale = 0;
      fprintf(stderr, "WARNING: X server doesn't support locale!\n");
    } else {
      x11_haslocale = 1;
      if (!XSetLocaleModifiers("@im=local")) {
        fprintf(stderr, "FATAL: XSetLocaleModifiers failed\n");
        exit(1);
      }
    }
  } else {
    fprintf(stderr, "WARNING: X server locale support disabled by user!\n");
    x11_haslocale = 0;
  }

  x11_dpy = XOpenDisplay(NULL);
  if (!x11_dpy) {
    fprintf(stderr, "FATAL: can't open display!\n");
    exit(1);
  }
  x11_screen = XDefaultScreen(x11_dpy);

  if (XDefaultDepth(x11_dpy, x11_screen) != 24 && XDefaultDepth(x11_dpy, x11_screen) != 32) {
    fprintf(stderr, "FATAL: invalid screen depth (%d)\n", XDefaultDepth(x11_dpy, x11_screen));
    exit(1);
  }

  #if FORCE_LOCALE
  if (olocale != NULL) {
    setlocale(LC_ALL, olocale);
    free(olocale);
  } else {
    setlocale(LC_ALL, "");
  }
  #endif

  xaXEmbed = XInternAtom(x11_dpy, "_XEMBED", False);
  xaVTSelection = XInternAtom(x11_dpy, "_SXED_SELECTION_", 0);
  xaClipboard = XInternAtom(x11_dpy, "CLIPBOARD", 0);
  xaUTF8 = XInternAtom(x11_dpy, "UTF8_STRING", 0);
  xaCSTRING = XInternAtom(x11_dpy, "C_STRING", 0);
  xaINCR = XInternAtom(x11_dpy, "INCR", 0);
  xaNetWMName = XInternAtom(x11_dpy, "_NET_WM_NAME", 0);
  xaTargets = XInternAtom(x11_dpy, "TARGETS", 0);
  xaWMProtocols = XInternAtom(x11_dpy, "WM_PROTOCOLS", 0);
  xaWMDeleteWindow = XInternAtom(x11_dpy, "WM_DELETE_WINDOW", 0);
  xaWorkArea = XInternAtom(x11_dpy, "_NET_WORKAREA", 0);
  xaXEmbed = XInternAtom(x11_dpy, "_XEMBED", False);
}


//==========================================================================
//
//  x11_parse_font_name
//
//  "Terminus Bold, 20"
//
//==========================================================================
static yterm_bool x11_parse_font_name (char *dest, size_t destsize, const char *fname) {
  if (!fname || !fname[0]) return 0;

  // if starts with a '-', this is X11 core font name
  if (fname[0] == '-') {
    if (strlen(fname) >= destsize) return 0;
    strcpy(dest, fname);
    return 1;
  }


  // this must be: "font,size"
  if (destsize < 16) return 0;

  // any family
  strcpy(dest, "-*-");
  uint32_t dpos = (uint32_t)strlen(dest);
  while (*fname && *fname > 0 && *fname <= 32) ++fname; // skip blanks

  // parse font name: one or two words ("Terminus Bold", "Helvetica Medium")
  // words will be lowercased
  // added: last word can be slant: "Regular, Italic, Oblique"
  int stars = 4;
  int was_char = 0;
  while (*fname) {
    uint8_t ch = *((const uint8_t *)fname++);
    if (ch == 32 || ch == 9) {
      if (!was_char) return 0;
      while (*fname && *fname > 0 && *fname <= 32) ++fname; // skip blanks
      if (fname[0] == ',') { ++fname; break; }
      if (!fname[0]) break;
      if (stars == 2) return 0;
      --stars;
      if (dpos >= (uint32_t)destsize) return 0;
      dest[dpos++] = '-';
      was_char = 0;
    } else if (ch == ',') {
      break;
    } else {
      if (dpos >= (uint32_t)destsize) return 0;
      if (ch >= 'A' && ch <= 'Z') ch = ch-'A'+'a';
      if ((ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') ||
          ch != '_')
      {
        dest[dpos++] = (char)ch;
        was_char = 1;
      } else {
        return 0;
      }
    }
  }

  // here, `stars` means "stars left to fill"

  if (stars == 4 && !was_char) return 0;


  if (stars == 3) { // add slant
    if (dpos+2 >= (uint32_t)destsize) return 0;
    dest[dpos++] = '-';
    dest[dpos++] = 'r';
    stars -= 1;
  } else if (stars == 2) { // check slant
    // this is slant
    char slant = 0;
    uint32_t bkx = dpos;
    while (bkx != 0 && dest[bkx - 1] != '-') bkx -= 1;
    //fprintf(stderr, "bkx=%u; dpos=%u; <%.*s>\n", bkx, dpos, (unsigned)dpos, dest);
    yterm_assert(bkx != 0);
    if (dpos - bkx == 7 && memcmp(dest + bkx, "regular", 7) == 0) {
      slant = 'r';
    } else if (dpos - bkx == 6 && memcmp(dest + bkx, "italic", 6) == 0) {
      slant = 'i';
    } else if (dpos - bkx == 7 && memcmp(dest + bkx, "oblique", 7) == 0) {
      slant = 'o';
    } else {
      return 0;
    }
    dpos = bkx;
    dest[dpos++] = slant;
  }

  // fill other stars
  for (; stars; --stars) {
    if (dpos+2 >= (uint32_t)destsize) return 0;
    dest[dpos++] = '-';
    dest[dpos++] = '*';
  }

  if (dpos >= (uint32_t)destsize) return 0;
  dest[dpos++] = '-';

  // size
  while (*fname && *fname > 0 && *fname <= 32) ++fname; // skip blanks

  int sz = 20;
  if (*fname) {
    sz = 0;
    while (*fname) {
      char ch = *fname++;
      if (ch < '0' || ch > '9') return 0;
      sz = sz*10+(ch-'0');
      if (sz > 255) return 0;
    }
  }

  while (*fname && *fname > 0 && *fname <= 32) ++fname; // skip blanks
  if (*fname) return 0;

  char tbuf[16];
  snprintf(tbuf, sizeof(tbuf), "%d", sz);
  if (dpos+(uint32_t)strlen(tbuf)+1 >= (uint32_t)destsize) return 0;
  strcpy(dest+dpos, tbuf);
  dpos += (uint32_t)strlen(tbuf);

  //fprintf(stderr, "!!!! 002: <%.*s>\n", (unsigned)dpos, dest);

  const char *suffix = "-*-*-*-*-*-iso10646-1";
  if (dpos+(uint32_t)strlen(suffix)+1 >= (uint32_t)destsize) return 0;
  strcpy(dest+dpos, suffix);
  dpos += (uint32_t)strlen(suffix);

  if (dpos >= (uint32_t)destsize) return 0;
  dest[dpos] = 0;

  //fprintf(stderr, "FONT: %s\n", dest);

  //-*-terminus-bold-*-*-*-20-*-*-*-*-*-iso10646-1
  return 1;
}


//==========================================================================
//
//  x11_free_fonts
//
//==========================================================================
static void x11_free_fonts (void) {
  yterm_assert(x11_dpy);
  if (x11_font.fst != NULL) XFreeFont(x11_dpy, x11_font.fst);
  if (x11_tabsfont.fst != NULL) XFreeFont(x11_dpy, x11_tabsfont.fst);
}


//==========================================================================
//
//  x11_setxhints
//
//==========================================================================
static void x11_setxhints (void) {
  XClassHint klass;
  if (opt_class == NULL || opt_class[0] == 0) {
    klass.res_class = "k8_yterm_x11";
    klass.res_name = "k8_yterm_x11";
  } else {
    klass.res_class = opt_class;
    klass.res_name = opt_class;
  }
  XWMHints wm;
  memset(&wm, 0, sizeof(wm));
  wm.flags = InputHint;
  wm.input = 1;

  XSizeHints size;
  memset(&size, 0, sizeof(size));
  size.flags = PSize|PMinSize|PResizeInc|PBaseSize|USSize;
  size.min_width = 4 * charWidth;
  size.min_height = 4 * charHeight + winDesiredTabsHeight;
  size.max_width = 32760; size.max_height = 32760; /* not used */
  size.width_inc = charWidth;
  size.width = winWidth;
  size.height = winRealHeight;
  size.base_width = size.min_width;
  size.base_height = size.min_height;
  size.height_inc = charHeight;

  XSetWMNormalHints(x11_dpy, x11_win, &size);
  XSetWMProperties(x11_dpy, x11_win, NULL, NULL, NULL, 0, &size, &wm, &klass);
  XSetWMProtocols(x11_dpy, x11_win, &xaWMDeleteWindow, 1);
  XResizeWindow(x11_dpy, x11_win, winWidth, winRealHeight);
  x11_change_title(NULL);
}


static yterm_bool want_buttons = 0;
static yterm_bool want_motion = 0;


//==========================================================================
//
//  CalcInputMask
//
//==========================================================================
YTERM_STATIC_INLINE long CalcInputMask (void) {
  return
    FocusChangeMask |
    KeyPressMask | KeyReleaseMask | KeymapStateMask |
    ExposureMask | VisibilityChangeMask | StructureNotifyMask |
    PropertyChangeMask |
    /*PointerMotionMask |*/ //TODO: do we need button-less motion reports at all?
    (want_motion ? Button1MotionMask | Button2MotionMask |
                   Button3MotionMask | Button4MotionMask | Button5MotionMask : 0) |
    (want_buttons ? ButtonPressMask | ButtonReleaseMask : 0) |
    /*EnterWindowMask | LeaveWindowMask | */
    0;
}


//==========================================================================
//
//  x11_enable_mouse
//
//==========================================================================
static void x11_enable_mouse (yterm_bool buttons, yterm_bool motion) {
  buttons = !!buttons; motion = !!motion;
  if (want_buttons != buttons || want_motion != motion) {
    want_buttons = buttons;
    want_motion = motion;
    #if 0
    fprintf(stderr, "new mouse: btn=%d; motion=%d\n", want_buttons, want_motion);
    #endif
    XSelectInput(x11_dpy, x11_win, CalcInputMask());
  }
}


//==========================================================================
//
//  x11_create_cwin_x11
//
//==========================================================================
static void x11_create_cwin_x11 (void) {
  int ax0 = 0;
  int ay0 = 0;
  int awdt = opt_twidth;
  int ahgt = opt_theight;
  int tbarh = 0;

  char xfontname[256];
  XSetWindowAttributes attrs;
  Window parent;
  //XColor blackcolor = { 0, 0, 0, 0, 0, 0 };

  // init mono font
  if (!x11_parse_font_name(xfontname, sizeof(xfontname), opt_mono_font)) {
    xfontname[0] = 0;
  }
  //fprintf(stderr, "PARSED MONO FONT: %s\n", xfontname);
  if (!x11_initfont(&x11_font, xfontname, 1)) {
    if (!x11_initfont(&x11_font, "-*-fixed-*-*-*-*-20-*-*-*-*-*-*-*", 1)) {
      fprintf(stderr, "FATAL: can't load monospace font '%s'\n", opt_mono_font);
      exit(1);
    }
  }

  // init tabbar font
  if (opt_enable_tabs) {
    if (!x11_parse_font_name(xfontname, sizeof(xfontname), opt_tabs_font)) {
      xfontname[0] = 0;
    }
    //fprintf(stderr, "PARSED TABS FONT: %s\n", xfontname);
    if (!x11_initfont(&x11_tabsfont, xfontname, 0)) {
      fprintf(stderr, "FATAL: can't load tabbar font '%s'\n", opt_tabs_font);
      exit(1);
    }
    tbarh = x11_tabsfont.ascent + x11_tabsfont.descent + 1;
  } else {
    memset(&x11_tabsfont, 0, sizeof(x11_tabsfont));
    tbarh = 0;
  }

  /* XXX: Assuming same size for bold font */
  //cwin->charWidth = x11_font.rbearing-x11_font.lbearing;
  charWidth = x11_font.width;
  charHeight = x11_font.ascent + x11_font.descent;

  // get working area size
  int wa_width = 800;
  int wa_height = 600;
  {
    Window root = XRootWindow(x11_dpy, x11_screen);
    Atom aType;
    int format;
    unsigned long itemCount;
    unsigned long bytesAfter;
    int *propRes = NULL;
    int status = XGetWindowProperty(x11_dpy, root, xaWorkArea, 0, 4, /*False*/0,
                                    AnyPropertyType, &aType, &format, &itemCount,
                                    &bytesAfter, (unsigned char**)&propRes);
    if (status >= Success) {
      if (propRes) {
        //x = propRes[0];
        //y = propRes[1];
        wa_width = propRes[2];
        wa_height = propRes[3];
        XFree(propRes);
        //fprintf(stderr, "WA: %dx%d\n", wa_width, wa_height);
      }
    }
  }

  // try to fit the window into the working area
  if (awdt <= 0) {
    awdt = 80;
    if (awdt * charWidth >= wa_width) awdt = max2(1, (wa_width - 8) / charWidth);
  }
  if (ahgt <= 0) {
    ahgt = 24;
    if (ahgt * charHeight >= wa_height) ahgt = max2(1, (wa_height - 8) / charHeight);
  }

  if (ax0 < 0) ax0 = 0;
  if (ay0 < 0) ay0 = 0;

  // window - default size
  winWidth = awdt * charWidth;
  winHeight = ahgt * charHeight;
  winDesiredTabsHeight = tbarh;
  winRealHeight = winHeight + tbarh;

  memset(&attrs, 0, sizeof(attrs));
  attrs.background_pixel = None;
  attrs.border_pixel = None;
  attrs.bit_gravity = NorthWestGravity;
  attrs.event_mask = CalcInputMask();
  attrs.colormap = XDefaultColormap(x11_dpy, x11_screen);
  parent = XRootWindow(x11_dpy, x11_screen);
  if (x11_embed != None) {
    fprintf(stderr, "X-EMBED: wid=%d\n", (int)x11_embed);
    parent = x11_embed;
  }
  x11_win = XCreateWindow(x11_dpy, parent, ax0, ay0,
      winWidth, winRealHeight, 0, XDefaultDepth(x11_dpy, x11_screen), InputOutput,
      XDefaultVisual(x11_dpy, x11_screen),
      CWBackPixel|CWBorderPixel|CWBitGravity|CWEventMask|CWColormap,
      &attrs);
  x11_setxhints();

  // input method
  if (x11_haslocale >= 0) {
    if (!(x11_xim = XOpenIM(x11_dpy, NULL, NULL, NULL))) {
      if (x11_haslocale) {
        if (!XSetLocaleModifiers("@im=local")) {
          fprintf(stderr, "FATAL: XSetLocaleModifiers failed\n");
          exit(1);
        }
        if (!(x11_xim = XOpenIM(x11_dpy, NULL, NULL, NULL))) {
          if (!XSetLocaleModifiers("@im=")) {
            fprintf(stderr, "FATAL: XSetLocaleModifiers failed\n");
            exit(1);
          }
          if (!(x11_xim = XOpenIM(x11_dpy, NULL, NULL, NULL))) {
            fprintf(stderr, "FATAL: XOpenIM() failed\n");
            exit(1);
          }
        }
      }
    }
    if (x11_xim) {
      x11_xic = XCreateIC(x11_xim,
        XNInputStyle, XIMPreeditNothing|XIMStatusNothing,
        XNClientWindow, x11_win,
        XNFocusWindow, x11_win,
        NULL);
      if (!x11_xic) {
        fprintf(stderr, "FATAL: XCreateIC failed\n");
        exit(1);
      }
    }
  }

  // x11_gc
  XGCValues gval;
  gval.font = x11_font.fst->fid;
  x11_gc = XCreateGC(x11_dpy, (Drawable)x11_win, GCFont, &gval);
  XSetGraphicsExposures(x11_dpy, x11_gc, True);

  #if 0
  if (mXTextCursor == None) {
    mXTextCursor = XCreateFontCursor(x11_dpy, /*XC_xterm*/152);
  }

  if (mXBlankCursor == None) {
    char cmbmp[1] = {0};
    Pixmap pm;
    pm = XCreateBitmapFromData(x11_dpy, (Drawable)x11_win, cmbmp, 1, 1);
    mXBlankCursor = XCreatePixmapCursor(x11_dpy, pm, pm, &blackcolor, &blackcolor, 0, 0);
    XFreePixmap(x11_dpy, pm);
  }
  #endif

  if (mXDefaultCursor == None) {
    mXDefaultCursor = XCreateFontCursor(x11_dpy, /*XC_X_cursor*/ 68/*XC_left_ptr*/);
  }

  #if 0
  XDefineCursor(x11_dpy, x11_win, mXTextCursor);
  #else
  // force default cursor on us, because why not?
  XDefineCursor(x11_dpy, x11_win, mXDefaultCursor);
  #endif
  //XStoreName(x11_dpy, x11_win, "YTERM");
  x11_lasttitle[0] = 0;
  x11_change_title(NULL);

  XSetBackground(x11_dpy, x11_gc, lastBG);
  XSetForeground(x11_dpy, x11_gc, lastFG);

  XMapWindow(x11_dpy, x11_win);

  //XSetICFocus(x11_xic);

  #if 0
  fprintf(stderr, "INITIAL WSIZE: %dx%d (real=%d; tabs=%d)\n", winWidth, winHeight, winRealHeight,
          winDesiredTabsHeight);
  #endif
}
