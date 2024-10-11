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
/* O_TMPFILE */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

// do not turn this on, you don't need it
//#define USELESS_DEBUG_FEATURES

#include "common.h"
#include "utf.h"
#include "cellwin.h"

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>


#include "common.c"
#include "utf.c"
#include "cellwin.c"

#define HAVE_CONFIG_H
#include "xsel/xsel.c"


// ////////////////////////////////////////////////////////////////////////// //
//#define FORCE_LOCALE
//#define DUMP_X11_EVENTS
//#define DUMP_RW_TIMEOUTS
//#define DUMP_READ_AMOUNT
//#define DUMP_WRITE_AMOUNT
//#define DUMP_WRITE_SHRINK
//#define DUMP_WRITE_GROW


// ////////////////////////////////////////////////////////////////////////// //
// bytes to reserve for write buffer
// write buffer may grow, but will eventually be shrinked to this value
#define WRITE_BUF_SIZE  (128)


// ////////////////////////////////////////////////////////////////////////// //
#define DEFAULT_MONO_FONT  "Terminus Bold, 20"
#define DEFAULT_TABS_FONT  "Helvetica Medium Regular, 14"


// ////////////////////////////////////////////////////////////////////////// //
typedef struct MenuWindow_t MenuWindow;


// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  int ascent;
  int descent;
  short lbearing;
  short rbearing;
  int width;
  XFontStruct *fst;
} XFont;


// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  char **argv;
  int argc;
  char *path; // can be NULL or empty
} ExecData;


// ////////////////////////////////////////////////////////////////////////// //
// terminal mode flags (bitmask)
enum {
  YTERM_MODE_WRAP        = 1u<<0, // cursor wrapping enabled
  YTERM_MODE_INSERT      = 1u<<1, // insert chars instead of overwriting?
  YTERM_MODE_APPKEYPAD   = 1u<<2, //TODO: currently does nothing (will not implement)
  YTERM_MODE_CRLF        = 1u<<3, // LF should print CR too
  YTERM_MODE_MOUSEBTN    = 1u<<4, // want mouse buttons reporting
  YTERM_MODE_MOUSEMOTION = 1u<<5, // want mouse motion reporting
  YTERM_MODE_REVERSE     = 1u<<6, // all output is inversed
  YTERM_MODE_BRACPASTE   = 1u<<7, // "bracketed paste" mode enabled
  YTERM_MODE_FOCUSEVT    = 1u<<8, // send focus/unfocus events?
  YTERM_MODE_DISPCTRL    = 1u<<9, //TODO: not implemented yet
  YTERM_MODE_GFX0        = 1u<<10, // font G0 is in gfx mode
  YTERM_MODE_GFX1        = 1u<<11, // font G1 is in gfx mode
  YTERM_MODE_FORCED_UTF  = 1u<<12, // is UTF-8 mode forced via ESC # G / ESC # 8 ?
  YTERM_MODE_VT220_KBD   = 1u<<13, // xterm VT220 keyboard emulation (Home/End keys)? (reset on PID change)
  YTERM_MODE_XTERM       = 1u<<14, // is in xterm mode for Home/End keys? (reset on PID change)
  YTERM_MODE_NO_ALTSCR   = 1u<<15, // is switching to alternate screen disabled?
  YTERM_MODE_ORIGIN      = 1u<<16, // vertical cursor position is relative to scroll region

  YTERM_MODE_MOUSE = (YTERM_MODE_MOUSEBTN | YTERM_MODE_MOUSEMOTION),

  // GFX0 means "charset 0 is in gfx mode"
  // GFX1 means "charset 1 is in gfx mode"
  // `charset` field contains the same constant, so it is
  // easy to check for gfx mode with `term->mode & term->charset`
  YTERM_MODE_GFX_MASK = (YTERM_MODE_GFX0 | YTERM_MODE_GFX1),
};

// ////////////////////////////////////////////////////////////////////////// //
// maxumum number of arguments in escape seqence for VT-100
#define YTERM_ESC_ARG_SIZ    (16)


// ////////////////////////////////////////////////////////////////////////// //
// xmodmap -pm
enum {
  Mod_Ctrl = ControlMask,
  Mod_Shift = ShiftMask,
  Mod_Alt = Mod1Mask,
  Mod_NumLock = Mod2Mask,
  Mod_AltGr = Mod3Mask,
  Mod_Hyper = Mod4Mask,
};

enum {
  MKey_Ctrl_L  = 1u<<0,
  MKey_Ctrl_R  = 1u<<1,
  MKey_Alt_L   = 1u<<2,
  MKey_Alt_R   = 1u<<3,
  MKey_Shift_L = 1u<<4,
  MKey_Shift_R = 1u<<5,
};

// pressed/released button number
enum {
  YTERM_MR_NONE,
  // WARNING! numbers are important!
  YTERM_MR_LEFT,
  YTERM_MR_MIDDLE,
  YTERM_MR_RIGHT,
};

// event type
enum {
  YTERM_MR_UP,
  YTERM_MR_DOWN,
  YTERM_MR_MOTION,
};

// for `deadstate`
enum {
  DS_ALIVE,
  // waiting for a keypress (enter or space)
  DS_WAIT_KEY,
  // waiting for a keypress (enter, space or esc)
  DS_WAIT_KEY_ESC,
  // dead, remove from the list on the next main loop iteration
  DS_DEAD,
};

// selection block type
enum {
  SBLOCK_NONE = 0, // selection mode is inactive
  SBLOCK_STREAM,
  SBLOCK_LINE,
  SBLOCK_RECT,
};


// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  int fd; // history file fd (-1 if none yet)
  int width; // history line width
  int lcount; // number of history lines
  uint8_t *wrbuffer;
  uint32_t wrbufferSize;
  uint32_t wrbufferUsed;
  off_t wrbufferOfs;
} HistoryFile;


// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  int x, y;
  // "last column flag", used for delayed wrapping
  yterm_bool lastColFlag;
  // set on ant cursor repositioning ESC; reset in `term_putc()`
  // used to reset "autowrap" flag on the line we're changing
  yterm_bool justMoved;
} CursorPos;

typedef struct Message_t {
  char *message;
  uint64_t time; // either die time, our timeout for yet-to-show messages
  struct Message_t *next;
} Message;

typedef struct {
  CellBuffer cbuf;
  CursorPos curpos;
  CursorPos cursaved; // saved cursor position
  CharCell currattr; // for attributes and flags
  yterm_bool curhidden;
  // scroll top and bottom lines (inclusive)
  int scTop, scBot;
  // for "ESC 7" and "ESC 8"
  CharCell attrsaved;
  uint32_t charsetsaved;
  uint32_t modesaved; // charsets
} TermScreen;

#define TAB_MAX_TEXT_LEN  (128)

// tab info
typedef struct {
  int posx; // position in window; may be out of window
  int width; // tab width, including a one-pixel right delimiter
  int numwidth; // tab num text width
  int textwidth; // tab text width
  int numlen;
  int textlen;
  char textnum[8];
  XChar2b text[TAB_MAX_TEXT_LEN];
} TabInfo;

typedef struct term_t {
  TermScreen *wkscr;  // check this to determine which buffer is active
  // main and alternate screens
  TermScreen main;
  TermScreen alt;
  yterm_bool active;  // is this terminal active? inactive terminals are invisible
  uint32_t mode;      // terminal mode flags
  uint32_t charset;   // YTERM_MODE_GFX0 or YTERM_MODE_GFX1
  // we will not issue `XCopyArea()` immediately, but accumulate the value instead
  // otherwise it looks bad with deferred updates
  // negative is up, positive is down, `0x7fffffff` is "locked"
  int scroll_count;
  // we also need to remember the scrolling area
  int scroll_y0, scroll_y1;
  struct {
    int state; // escape processor state
    int priv;  // "private CSI sequence" flag (-1, 0, 1); OSC: seen accum digits
    int cmd;   // final CSI char (command); OSC command accumulator
    int argv[YTERM_ESC_ARG_SIZ]; // -1 means "omited"
    int argc;  // 0 if we haven't seen any args yet
    // for OSC, and for queries
    char sbuf[256];
    int sbufpos;
  } escseq;
  // child PID
  struct {
    pid_t pid;
    int cmdfd; // terminal control fd for child
  } child;
  struct {
    pid_t pgrp;
    char last[256];
    yterm_bool custom; // custom title?
    uint64_t next_check_time; // next title check time
  } title;
  int deadstate; // see `DS_xxx`
  // write buffer
  struct {
    char *buf;
    uint32_t size;
    uint32_t used;
  } wrbuf;
  // read DFA (for one UTF-8 char)
  uint32_t rdcp;
  struct {
    int mode; // mouse report mode: 1000, 1005, 1006, 1015
    int lastx, lasty; // last reported coords
    int lastbtn; // last pressed button
  } mreport;
  // "selection mode" support
  struct {
    int cx, cy; // selection cursor position in screen
    int sback; // top history line index, counting from 1; 1 is the *LAST* line
    int x0, y0, x1, y1; // currently selected block; inclusive; from the bottom of cbuf and up
    int blocktype; // `SBLOCK_xxx`
    HistoryFile file;
    yterm_bool enabled;
    yterm_bool inprogress; // we're currently selecting
    yterm_bool locked; // do not leave selection mode after copying text
  } history;
  Message *osd_messages;
  // last activation time, so we could return to the previous term
  uint64_t last_acttime;
  // tab info
  TabInfo tab;
  // local color mode, or -1
  int colorMode;
  // report modifiers as "\x1b[1;<mods>Y"?
  yterm_bool reportMods;
  uint32_t lastModKeys; // only mods
  // local mouse reports, or -1
  int mouseReports;
  // local escesc, or -1
  int escesc;
  // saved values (was changed by private ESC)
  int savedValues;
  // linked list of terminals
  struct term_t *prev;
  struct term_t *next;
} Term;


// ////////////////////////////////////////////////////////////////////////// //
// handy macros ;-)

#define TERM_CBUF(tt_)   (&((tt_)->wkscr->cbuf))
#define TERM_CPOS(tt_)   (&((tt_)->wkscr->curpos))
#define TERM_CATTR(tt_)  (&((tt_)->wkscr->currattr))
#define TERM_SCTOP(tt_)  ((tt_)->wkscr->scTop)
#define TERM_SCBOT(tt_)  ((tt_)->wkscr->scBot)

#define TERM_CURHIDDEN(tt_)   ((tt_)->wkscr->curhidden)
#define TERM_CURVISIBLE(tt_)  (!((tt_)->wkscr->curhidden))

#define TERM_IN_SELECTION(tt_)  ((tt_)->history.blocktype != SBLOCK_NONE)


// ////////////////////////////////////////////////////////////////////////// //
static Display *x11_dpy = NULL;
static int x11_screen;
static Window x11_win = None;
static Window x11_embed = None;
static XFont x11_font;
static XFont x11_tabsfont;
static GC x11_gc;
static XIM x11_xim;
static XIC x11_xic;
static int koiLocale = 0;
//static Cursor mXTextCursor = None; // text cursor
static Cursor mXDefaultCursor = None; // 'default' cursor
//static Cursor mXBlankCursor = None;
static uint64_t lastBellTime = 0;

static Atom xaXEmbed;
static Atom xaVTSelection;
static Atom xaClipboard;
static Atom xaUTF8;
static Atom xaCSTRING;
static Atom xaINCR;
static Atom xaTargets;
static Atom xaNetWMName;
static Atom xaWMProtocols;
static Atom xaWMDeleteWindow;
static Atom xaWorkArea;

static uint32_t lastBG = 0xffffffffU, lastFG = 0xffffffffU;
// if high byte is not zero, don't show
static uint32_t curColorsFG[2] = { 0x005500, 0x005500 };
// the last is for unfocused cursor
static uint32_t curColorsBG[3] = { 0x00ff00, 0x00cc00, 0x009900 };

static int winHeight = 0, winWidth = 0;
static int charWidth = 0, charHeight = 0;
// tabs height can be bigger than requested due to WM window resizing
// it is calculated in configure notify handler
static int winTabsHeight = 0;
// calculated (desired) tabs height
static int winDesiredTabsHeight = 0;
// "real" height includes tabbar; tabbar is always at bottom
static int winRealHeight = 0;

static int curPhase = 0;

static int quitMessage = 0;
static int winVisible = 0;
static int winFocused = 0;
static int winMapped = 0;
static char x11_lasttitle[1024] = {0};
static int x11_haslocale = 0;

static Term *currterm = NULL;
static Term *termlist = NULL;

static char *xrm_app_name = NULL;

static MenuWindow *curr_menu = NULL;

// to render scrolled history, we will use the separate buffer,
// because main buffer holds current terminal contents
static CellBuffer hvbuf;

static int childsigfd;
static ExecData first_run_ed;
static uint64_t first_run_xtime = 0;
// wait for some time before executing the initial command
// this hack is to allow WM to setup yterm size
// sadly, there is no X11 event to tell us that
// WM is done configuring the window, hence this hack
// time in msecs, and affects only "-e"
static int opt_exec_delay = 50;

// render FPS
static uint64_t next_redraw_time = 0;
static yterm_bool need_update_tabs = 0;

// hotswap support
static char exe_path[4096];

static yterm_bool xrm_reloading = 0;


// ////////////////////////////////////////////////////////////////////////// //
enum {
  TT_ANY,
  TT_RXVT,
  TT_XTERM,
};

static char *opt_term = NULL;
static char *opt_class = NULL;
static char *opt_title = NULL;
static char *opt_mono_font = NULL;
static char *opt_tabs_font = NULL;
static int opt_twidth = 80;
static int opt_theight = 24;
static yterm_bool opt_enable_tabs = 1;
static yterm_bool opt_cycle_tabs = 0;
// send ESC ESC on escape, and single ESC on alt-escape?
static yterm_bool opt_esc_twice = 1;
static yterm_bool opt_mouse_reports = 0;
static int opt_cur_blink_time = 700; // 0: don't blink
static int opt_term_type = TT_RXVT;
static int opt_tabs_visible = 6;
static yterm_bool opt_history_enabled = 1;
static int opt_fps = 40;
static yterm_bool opt_terminus_gfx = 1;
// wait for some time before setting terminal size
// this hack is to allow WM to setup yterm size
// sadly, there is no X11 event to tell us that
// WM is done configuring the window, hence this hack
// time in msecs, and affects only "-e" and hotswap
static int opt_winch_delay = 150;
// if 0, then we should init it with the above
static uint64_t winch_allowed_time = 0;
static yterm_bool opt_workaround_vim_idiocity = 0;

// xsel command lines for 3 clipboard types
static char *opt_paste_from[3] = {NULL, NULL, NULL};
static char *opt_copy_to[3] = {NULL, NULL, NULL};

static int opt_dump_fd = -1;
static yterm_bool opt_dump_fd_enabled = 1;
static yterm_bool opt_dump_esc_enabled = 0;
#ifdef USELESS_DEBUG_FEATURES
static yterm_bool opt_debug_slowdown = 0;
#endif
static yterm_bool opt_vttest = 0;
// when the last terminal is closed, wait for a keypress
static yterm_bool opt_wait_last_term = 0;

static uint32_t opt_osd_menu_shadow_color = 0x000000;
static int opt_osd_menu_shadow_xofs = 6;
static int opt_osd_menu_shadow_yofs = 6;

static yterm_bool opt_amber_tint = 1;

static yterm_bool opt_debug_perform_hotswap = 0;
static int opt_debug_hotswap_fd = -1;
#define IS_HOTSWAPPING()  (opt_debug_hotswap_fd >= 0)

static char txrdp[256];
static char txexe[4096];
static char txpath[4096];


// ////////////////////////////////////////////////////////////////////////// //
static void term_write (Term *term, const char *str);
static void term_release_alt_buffer (Term *term);
static void one_term_write (Term *term);
static Term *term_create_new_term (void);
static void term_activate (Term *term);
static void xrm_load_options (void);
static void exec_hotswap (void);
static void xrm_seal_option_by_ptr (const void *ptr);
static yterm_bool xrm_is_option_sealed_by_ptr (const void *ptr);
static void do_paste (Term *term, int mode);
static void do_copy_to (Term *term, int mode);
static Region x11_render_osd (const char *msg, Region clipreg);
static Region x11_render_osd_menus (MenuWindow *menu);

YTERM_STATIC_INLINE void force_tab_redraw (void) { need_update_tabs = 1; }

YTERM_STATIC_INLINE void force_frame_redraw (yterm_bool dirtyterm) {
  if (dirtyterm && currterm != NULL) cbuf_mark_all_dirty(TERM_CBUF(currterm));
  next_redraw_time = 0;
}


// ////////////////////////////////////////////////////////////////////////// //
#include "osd_menu.inc.c"


//==========================================================================
//
//  term_mouse_reports_enabled
//
//  enabled and requested
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool term_mouse_reports_enabled (const Term *term) {
  if (term == NULL || term->mouseReports == 0 ||
      (term->mouseReports == -1 && opt_mouse_reports == 0))
  {
    return 0;
  } else {
    if ((term->mode & YTERM_MODE_MOUSE) != 0 && !TERM_IN_SELECTION(term) &&
        term->deadstate == DS_ALIVE && term->child.cmdfd >= 0) {
      return 1;
    } else {
      return 0;
    }
  }
}


//==========================================================================
//
//  term_check_pgrp
//
//  this will reset sone terminal mode flags on program change
//
//==========================================================================
static void term_check_pgrp (Term *term, pid_t pgrp) {
  if (term != NULL && term->deadstate == DS_ALIVE) {
    if (term->child.cmdfd < 0) {
      term->mode &= ~(YTERM_MODE_VT220_KBD | YTERM_MODE_XTERM);
    } else {
      if (pgrp <= 0) pgrp = tcgetpgrp(term->child.cmdfd);
      if (pgrp > 0) {
        if (term->title.pgrp != pgrp) {
          if (term->title.pgrp != 0) {
            term->mode &= ~(YTERM_MODE_VT220_KBD | YTERM_MODE_XTERM);
            term_release_alt_buffer(term); // it is safe to call this
          }
          term->title.pgrp = pgrp;
        }
        force_tab_redraw();
      }
    }
  }
}


//==========================================================================
//
//  get_exe_path
//
//==========================================================================
static void get_exe_path (void) {
  exe_path[0] = 0;
  pid_t pid = getpid();
  if (pid <= 0) return;
  // get program current dir
  snprintf(txrdp, sizeof(txrdp), "/proc/%d/exe", (int)pid);
  ssize_t rd = readlink(txrdp, exe_path, sizeof(exe_path) - 1);
  if (rd < 1) {
    exe_path[0] = 0;
  } else {
    exe_path[rd] = 0;
  }
}


//==========================================================================
//
//  term_check_title
//
//==========================================================================
static void term_check_title (Term *term) {
  int fd;
  char *exe;
  char *path;
  ssize_t rd;

  if (term == NULL || term->deadstate != DS_ALIVE || term->child.cmdfd < 0) return;
  if (!winMapped) return;

  pid_t pgrp = tcgetpgrp(term->child.cmdfd);
  term_check_pgrp(term, pgrp);

  if (term->title.custom) return;

  term->title.last[0] = 0;

  if (pgrp < 1) goto error;

  // get program title
  snprintf(txrdp, sizeof(txrdp), "/proc/%d/cmdline", (int)pgrp);
  fd = open(txrdp, O_RDONLY|O_CLOEXEC);
  if (fd < 0) goto error;
  rd = read(fd, txexe, sizeof(txexe) - 1);
  close(fd);
  if (rd < 0) goto error;
  txexe[rd] = 0;

  exe = strrchr(txexe, '/');
  if (exe != NULL) exe += 1; else exe = txexe;

  // put into title
  snprintf(term->title.last, sizeof(term->title.last), "[%s]", exe);

  // get program current dir
  snprintf(txrdp, sizeof(txrdp), "/proc/%d/cwd", (int)pgrp);
  rd = readlink(txrdp, txpath, sizeof(txpath) - 1);
  if (rd < 0) return; // keep program name title
  txpath[rd] = 0;

  if (rd > 0) {
    // append to the title
    size_t tlen = strlen(term->title.last);
    if (tlen + 8 <= sizeof(term->title.last)) {
      // have room
      size_t plen = (size_t)rd;
      size_t nlen = tlen + plen + 1;
      if (nlen < sizeof(term->title.last)) {
        // ok
        snprintf(term->title.last, sizeof(term->title.last), "[%s] %s", exe, txpath);
      } else if (tlen + 8 < sizeof(term->title.last)) {
        size_t xleft = sizeof(term->title.last) - 5 - tlen;
        if (xleft < plen) {
          // should always be true
          path = txpath + (plen - xleft);
          snprintf(term->title.last, sizeof(term->title.last), "[%s] ...%s", exe, path);
        }
      }
    }
  }

  return;

error:
  snprintf(term->title.last, sizeof(term->title.last), "%s", opt_title);
}


//==========================================================================
//
//  term_lock_scroll
//
//  locks scrolling
//
//  scroll area will be marked dirty by the caller
//
//==========================================================================
YTERM_STATIC_INLINE void term_lock_scroll (Term *term) {
  if (term != NULL && term->scroll_count != 0x7fffffff) {
    // i don't know the area, so assume the whole scroll region
    if (term->scroll_count == 0) {
      term->scroll_y0 = term->wkscr->scTop;
      term->scroll_y1 = term->wkscr->scBot;
    }
    term->scroll_count = 0x7fffffff;
    #if 0
    fprintf(stderr, "COPY-SCROLL DISABLED.\n");
    #endif
  }
}


//==========================================================================
//
//  term_scroll_locked
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool term_scroll_locked (Term *term) {
  return (term != NULL && term->scroll_count == 0x7fffffff);
}


// ////////////////////////////////////////////////////////////////////////// //
#include "history.inc.c"
#include "x11_init.inc.c"
#include "x11_render.inc.c"
#include "x11_utils.inc.c"
#include "term_lib.inc.c"
#include "child_run.inc.c"
#include "x11_keypress.inc.c"
#include "clipboard.inc.c"
#include "tabbar.inc.c"

#include "hotswap.inc.c"

#include "x11_xrm.inc.c"
#include "cli_args.inc.c"


// ////////////////////////////////////////////////////////////////////////// //
/* XEMBED messages */
#define XEMBED_FOCUS_IN   (4)
#define XEMBED_FOCUS_OUT  (5)


//==========================================================================
//
//  x11_filter_event
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool x11_filter_event (XEvent *eventp) {
  if (XFilterEvent(eventp, (Window)0)) {
    return 1;
  }

  if (eventp->type == MappingNotify) {
    if (eventp->xmapping.request == MappingKeyboard ||
        eventp->xmapping.request == MappingModifier)
    {
      XRefreshKeyboardMapping(&eventp->xmapping);
    }
    return 1;
  }

  return 0;
}


//==========================================================================
//
//  x11_motion_report
//
//==========================================================================
static void x11_motion_report (XMotionEvent *event) {
  Term *term = currterm;

  if (!term_mouse_reports_enabled(term)) return;

  if ((event->state&(Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask)) != 0 &&
      /*(event->state&Mod_Shift) == 0*/1)
  {
    term_send_mouse(term, event->x / charWidth, event->y / charHeight,
                    YTERM_MR_MOTION, 0, event->state);
  }
}


//==========================================================================
//
//  x11_mouse_report
//
//==========================================================================
static void x11_mouse_report (XButtonEvent *event) {
  //TODO: check tab clicking here

  Term *term = currterm;

  if (!term_mouse_reports_enabled(term)) return;

  // 4 and 5 is a wheel
  if (/*(event->state&Mod_Shift) == 0 &&*/ event->button >= 1 && event->button <= 5) {
    term_send_mouse(term, event->x / charWidth, event->y / charHeight,
                    (event->type == ButtonPress ? YTERM_MR_DOWN : YTERM_MR_UP),
                    event->button, event->state);
  }
}


//==========================================================================
//
//  x11_event_process_other
//
//==========================================================================
static void x11_event_process (XEvent *event) {
  Term *term = currterm;

  switch (event->type) {
    case ClientMessage:
      if (event->xclient.message_type == xaWMProtocols) {
        if ((Atom)event->xclient.data.l[0] == xaWMDeleteWindow) {
          quitMessage = 1;
        }
      } else if (event->xclient.message_type == xaXEmbed && event->xclient.format == 32) {
        // see xembed specs http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
        if (event->xclient.data.l[1] == XEMBED_FOCUS_IN) {
          if (x11_xic) XSetICFocus(x11_xic);
          if (!winFocused) {
            winFocused = 1;
            term_invalidate_cursor(term);
            term_send_focus_event(term, 1);
            x11_set_urgency(0);
          }
        } else if (event->xclient.data.l[1] == XEMBED_FOCUS_OUT) {
          if (x11_xic) XUnsetICFocus(x11_xic);
          if (winFocused) {
            winFocused = 0;
            term_invalidate_cursor(term);
            term_send_focus_event(term, 0);
          }
        }
      }
      break;
    case KeyPress:
      x11_parse_keyevent(&event->xkey);
      break;
    case KeyRelease:
      x11_parse_keyevent_release(&event->xkey);
      break;
    case Expose:
      #ifdef DUMP_X11_EVENTS
      fprintf(stderr, "Expose: x=%d; y=%d; w=%d; h=%d\n",
              event->xexpose.x, event->xexpose.y,
              event->xexpose.width, event->xexpose.height);
      #endif
      /*if (event->xexpose.count == 0)*/ menu_mark_all_dirty(curr_menu);
      if (term != NULL) {
        // if scrolled, and trying to expose, render everything
        if (term->scroll_count != 0) {
          // do nothing if this is non-last event
          if (event->xexpose.count != 0) return;
          term->scroll_count = 0;
          // fix render coords
          int x0 = event->xexpose.x;
          int y0 = event->xexpose.y;
          int x1 = x0 + event->xexpose.width - 1;
          int y1 = y0 + event->xexpose.height - 1;
          x0 = 0; y0 = 0;
          x1 = max2(x1, winWidth * charWidth - 1);
          y1 = max2(y1, winHeight * charHeight - 1);
          if (x1 <= x0 || y1 <= y0) {
            cbuf_mark_all_dirty(TERM_CBUF(term));
            return;
          }
          // repaint everything
          event->xexpose.x = x0; event->xexpose.y = y0;
          event->xexpose.width = x1 - x0 + 1;
          event->xexpose.height = y1 - y0 + 1;
        }
        CALL_RENDER(
          renderArea(rcbuf, rcpos,
                     event->xexpose.x, event->xexpose.y,
                     event->xexpose.x + event->xexpose.width,
                     event->xexpose.y + event->xexpose.height);
        );
        if (event->xexpose.y + event->xexpose.height > winHeight) {
          repaint_tabs(event->xexpose.x, event->xexpose.width);
        }
      }
      break;
    case GraphicsExpose: // this is from area copying
      #ifdef DUMP_X11_EVENTS
      fprintf(stderr, "GraphicsExpose: x=%d; y=%d; w=%d; h=%d\n",
              event->xgraphicsexpose.x, event->xgraphicsexpose.y,
              event->xgraphicsexpose.width, event->xgraphicsexpose.height);
      #endif
      /*if (event->xgraphicsexpose.count == 0)*/ menu_mark_all_dirty(curr_menu);
      if (term != NULL) {
        #if 1
        CALL_RENDER(
          renderArea(rcbuf, rcpos,
                     event->xgraphicsexpose.x, event->xgraphicsexpose.y,
                     event->xgraphicsexpose.x + event->xgraphicsexpose.width,
                     event->xgraphicsexpose.y + event->xgraphicsexpose.height);
        );
        if (event->xgraphicsexpose.y + event->xgraphicsexpose.height > winHeight) {
          repaint_tabs(event->xgraphicsexpose.x, event->xgraphicsexpose.width);
        }
        #else
        SET_FG_COLOR(0xff0000);
        XFillRectangle(x11_dpy, (Drawable)x11_win, x11_gc,
                       event->xgraphicsexpose.x, event->xgraphicsexpose.y,
                       event->xgraphicsexpose.width, event->xgraphicsexpose.height);
        #endif
      }
      break;
    case VisibilityNotify:
      #ifdef DUMP_X11_EVENTS
      fprintf(stderr, "VisibilityNotify: %d\n", event->xvisibility.state);
      #endif
      if (event->xvisibility.state == VisibilityFullyObscured) {
        if (winVisible) {
          winVisible = 0;
          if (term != NULL) {
            term_lock_scroll(term);
            // no need to mark anything dirty here, because nothing is visible
            // if anything will become visible, X server will send proper expose events
            cbuf_mark_all_clean(TERM_CBUF(term));
          }
        }
      } else if (!winVisible) {
        winVisible = 1;
        if (winFocused) x11_set_urgency(0);
        // no need to have any dirty cells here, X server will send proper expose events
        // it is safe, because the window was fully invisible
        if (term != NULL) {
          term->scroll_count = 0; // we can accumulate scrolls again
          cbuf_mark_all_clean(TERM_CBUF(term));
        }
      }
      break;
    case FocusIn:
      #ifdef DUMP_X11_EVENTS
      fprintf(stderr, "FocusIn!\n");
      #endif
      if (x11_xic) XSetICFocus(x11_xic);
      if (!winFocused) {
        winFocused = 1;
        term_invalidate_cursor(term);
        term_send_focus_event(term, 1);
        x11_set_urgency(0);
      }
      break;
    case FocusOut:
      #ifdef DUMP_X11_EVENTS
      fprintf(stderr, "FocusOut!\n");
      #endif
      //!doneSelection();
      if (x11_xic) XUnsetICFocus(x11_xic);
      if (winFocused) {
        winFocused = 0;
        term_invalidate_cursor(term);
        term_send_focus_event(term, 0);
      }
      break;
    case MotionNotify:
      if (term != NULL) x11_motion_report(&event->xmotion);
      break;
    case ButtonPress:
    case ButtonRelease:
      if (term != NULL) x11_mouse_report(&event->xbutton);
      break;
    case SelectionNotify:
      // we got our requested selection, and should paste it
      //cwin_selection_came(cwin, &event->xselection);
      break;
    case SelectionRequest:
      //!prepareCopy(&event->xselectionrequest);
      break;
    case SelectionClear:
      //!clearSelection(&event->xselectionclear);
      break;
    case PropertyNotify:
      if (event->xproperty.state == PropertyNewValue) {
        /*
        char *pn = XGetAtomName(x11_dpy, event->xproperty.atom);
        fprintf(stderr, "PROP: new value for property '%s' (time=%u)\n", pn, (unsigned)event->xproperty.time);
        XFree(pn);
        */
        //if (event->xproperty.atom == xaVTSelection) cwin_more_selection(cwin);
      } else if (event->xproperty.state == PropertyDelete) {
        /*
        char *pn = XGetAtomName(x11_dpy, event->xproperty.atom);
        fprintf(stderr, "PROP: deleted property '%s' (time=%u)\n", pn, (unsigned)event->xproperty.time);
        XFree(pn);
        */
      } else {
        fprintf(stderr, "PROP: unknown event subtype!\n");
      }
      break;
    case ConfigureNotify:
      #if defined(DUMP_X11_EVENTS) || 0
      fprintf(stderr, "CONFIGURE! w=%d; h=%d; sent=%d\n",
              event->xconfigure.width / charWidth,
              event->xconfigure.height / charHeight,
              event->xconfigure.send_event);
      #endif
      winWidth = max2(charWidth, event->xconfigure.width);
      // calculate new window height
      winRealHeight = max2(charWidth, event->xconfigure.height);
      if (opt_enable_tabs) {
        winHeight = (winRealHeight - winDesiredTabsHeight) / charHeight * charHeight;
        winHeight = max2(charHeight, winHeight);
        // calculate real tabs height
        winTabsHeight = max2(0, winRealHeight - winHeight);
        // if our tabs are smaller than desired, try to allocate more room for them
        if (winTabsHeight < winDesiredTabsHeight &&
            winRealHeight - winDesiredTabsHeight > charHeight * 2)
        {
          // we have enough room
          winHeight = winRealHeight - winDesiredTabsHeight;
          winHeight = (winRealHeight - winDesiredTabsHeight) / charHeight * charHeight;
          yterm_assert(winHeight >= charHeight);
        }
      } else {
        winTabsHeight = 0;
        winHeight = max2(1, winRealHeight / charHeight * charHeight);
      }
      force_tab_redraw();
      #if 0
      fprintf(stderr, "NEW WSIZE: %dx%d (real=%d; tabs=%d); evt: %dx%d\n",
              winWidth, winHeight, winRealHeight, winDesiredTabsHeight,
              event->xconfigure.width, event->xconfigure.height);
      #endif
      // first run commandline
      if (first_run_ed.argc != 0 && term != NULL) {
        first_run_xtime = yterm_get_msecs() + max2(0, opt_exec_delay);
        /*
        const yterm_bool rres = term_exec(term, &first_run_ed);
        #if 0
        fprintf(stderr, "FIRST-RUN: <%s> (fd=%d; pid=%u)\n", first_run_ed.argv[0],
                term->child.cmdfd, (unsigned)term->child.pid);
        #endif
        execsh_free(&first_run_ed);
        if (rres == 0) {
          fprintf(stderr, "FATAL: cannot run initial program!\n");
          quitMessage = 666;
        }
        */
      }
      break;
    case MapNotify:
      #ifdef DUMP_X11_EVENTS
      fprintf(stderr, "MapNotify!\n");
      #endif
      winMapped = 1;
      if (winVisible) {
        fprintf(stderr, "WARNING: WTF?! window is just mapped, yet marked as visible!\n");
        winVisible = 0;
      }
      // recheck all terminal titles
      for (Term *tt = termlist; tt != NULL; tt = tt->next) tt->title.next_check_time = 0;
      if (term != NULL) {
        cbuf_mark_all_clean(TERM_CBUF(term));
      }
      break;
    case UnmapNotify:
      winMapped = 0;
      // just in case
      winVisible = 0;
      if (term != NULL) {
        term_lock_scroll(term);
        cbuf_mark_all_clean(TERM_CBUF(term));
      }
      break;
    case DestroyNotify:
      winVisible = 0;
      winFocused = 0;
      winMapped = 0;
      x11_win = None;
      break;
    default:
      break;
  }
}


//==========================================================================
//
//  one_term_write
//
//  usually we don't have a lot of writes, so don't bother looping.
//  writing a lot of data can happen on clipboard paste, but in this
//  case we are blocked anyway.
//
//==========================================================================
static void one_term_write (Term *term) {
  yterm_assert(term != NULL);
  if (term->wrbuf.used != 0 && term_can_write(term)) {
    #ifdef DUMP_WRITE_AMOUNT
    fprintf(stderr, "NEED TO WRITE: %u\n", term->wrbuf.used);
    #endif
    ssize_t wr = write(term->child.cmdfd, term->wrbuf.buf, term->wrbuf.used);
    #ifdef DUMP_WRITE_AMOUNT
    fprintf(stderr, "WROTE: %d / %u\n", (int)wr, term->wrbuf.used);
    #endif
    if (wr < 0) {
      // `EAGAIN` should not happen, but better be safe than sorry
      if (errno != EINTR && errno != EAGAIN) {
        fprintf(stderr, "OOPS! cannot write data to terminal!\n");
        term_close_fd(term);
      }
    } else if (wr == 0) {
      // wutafuck?! this should not happen at all
      fprintf(stderr, "OOPS! terminal seems to close the connection (write)!\n");
      term_close_fd(term);
    } else {
      yterm_assert((uint32_t)wr <= term->wrbuf.used);
      const uint32_t dleft = term->wrbuf.used - (uint32_t)wr;
      if (dleft != 0) {
        // move data
        memmove(term->wrbuf.buf, term->wrbuf.buf + (size_t)wr, dleft);
      }
      term->wrbuf.used -= (uint32_t)wr;
      //if (term->wrbuf.used != 0) again = 1;
    }
  }

  // shrink buffer
  if (term->wrbuf.size > WRITE_BUF_SIZE) {
    const uint32_t nsz = max2(WRITE_BUF_SIZE,
                              (term->wrbuf.size > 4096 ? 4096 : term->wrbuf.size / 2));
    if (term->wrbuf.used <= nsz) {
      #ifdef DUMP_WRITE_SHRINK
      fprintf(stderr, "SHRINKING: %u -> %u (used: %u)\n",
              term->wrbuf.size, nsz, term->wrbuf.used);
      #endif
      char *nbx = realloc(term->wrbuf.buf, nsz);
      if (nbx != NULL) {
        term->wrbuf.buf = nbx;
        term->wrbuf.size = nsz;
      }
    }
  }
}


//==========================================================================
//
//  all_term_write
//
//==========================================================================
static void all_term_write (void) {
  for (Term *term = termlist; term != NULL; term = term->next) {
    one_term_write(term);
  }
}


//==========================================================================
//
//  one_term_read
//
//  terminal should be valid, and checked for readability
//
//==========================================================================
static void one_term_read (Term *term) {
  if (term->child.cmdfd >= 0 && !TERM_IN_SELECTION(term)) {
    char rdbuf[4096];
    ssize_t rd = read(term->child.cmdfd, rdbuf, sizeof(rdbuf));
    #ifdef DUMP_READ_AMOUNT
    fprintf(stderr, "READ(%p): %d\n", term, (int)rd);
    #endif
    if (rd < 0) {
      // `EAGAIN` should not happen, but better be safe than sorry
      if (errno != EINTR && errno != EAGAIN) {
        if (errno != EIO) {
          fprintf(stderr, "OOPS! cannot read data from terminal! (errno=%d: %s)\n",
                  errno, strerror(errno));
          term_close_fd(term);
        }
      }
    } else if (rd == 0) {
      // wutafuck?! this should not happen at all
      fprintf(stderr, "OOPS! terminal seems to close the connection (read)!\n");
      term_close_fd(term);
    } else if (koiLocale && (term->mode & YTERM_MODE_FORCED_UTF) == 0) {
      // read KOI
      ssize_t pos = 0;
      while (pos < rd) {
        term->rdcp = yterm_koi2uni(((const unsigned char *)rdbuf)[pos]); pos += 1;
        term_process_char(term, term->rdcp);
      }
    } else {
      // read UTF
      ssize_t pos = 0;
      while (pos < rd) {
        term->rdcp = yterm_utf8d_consume_ex(term->rdcp, rdbuf[pos]); pos += 1;
        if (yterm_utf8_valid_cp(term->rdcp)) {
          term_process_char(term, term->rdcp);
        } else if (yterm_utf8d_fuckedup(term->rdcp)) {
          term_putc(term, YTERM_UTF8_REPLACEMENT_CP);
        }
      }
    }
  }
}


//==========================================================================
//
//  all_term_read
//
//==========================================================================
static void all_term_read (uint64_t rdett) {
  #ifdef DUMP_READ_AMOUNT
  int was_more = 0;
  #endif
  int has_more;
  uint64_t ctt;
  if (rdett != 0) rdett -= 1;
  do {
    has_more = 0;
    Term *term = termlist;
    while (term != NULL) {
      if (!TERM_IN_SELECTION(term) && term_can_read(term)) {
        one_term_read(term);
        if (!has_more && term->child.cmdfd >= 0) {
          has_more = term_can_read(term);
          #ifdef DUMP_READ_AMOUNT
          if (has_more) {
            was_more = 1;
            fprintf(stderr, "...has more!\n");
          }
          #endif
        }
      }
      term = term->next;
    }
    ctt = (has_more ? yterm_get_msecs() : rdett + 1);
    #ifdef DUMP_RW_TIMEOUTS
    fprintf(stderr, "RDETT: %llu; ctt: %llu\n", rdett, ctt);
    #endif
  } while (has_more && ctt <= rdett);
  #ifdef DUMP_READ_AMOUNT
  if (was_more) fprintf(stderr, ":::reading done!\n");
  #endif
}


//==========================================================================
//
//  remove_menus_for_term
//
//==========================================================================
static void remove_menus_for_term (Term *term) {
  MenuWindow *menu = curr_menu;
  MenuWindow *next = NULL;
  while (menu != NULL) {
    if (menu->term == term) {
      // remove this menu
      // mark dirty first, because we may need to redraw tabbar
      menu_mark_all_dirty(curr_menu);
      if (next != NULL) {
        next->prev = menu->prev;
        menu_free_window(menu);
        menu = next->prev;
      } else {
        curr_menu = menu->prev;
        menu_free_window(menu);
        menu = curr_menu;
      }
    } else {
      next = menu;
      menu = menu->prev;
    }
  }
}


//==========================================================================
//
//  remove_dead_terminals
//
//==========================================================================
static void remove_dead_terminals (void) {
  Term *term = termlist;
  while (term != NULL) {
    Term *cc = term; term = cc->next;
    if (cc->deadstate == DS_DEAD) {
      remove_menus_for_term(cc);
      // last one?
      if (opt_wait_last_term && termlist->next == NULL) {
        opt_wait_last_term = 0;
        cc->deadstate = DS_WAIT_KEY_ESC;
        term_simple_print_ensure_nl(cc);
        term_simple_print(cc, "\x1b[0;40;37;1m\x1b[K[PRESS ENTER, SPACE OR ESC]\x1b[0m");
      } else {
        // other checks
        if (cc == currterm) currterm = NULL; // will be fixed later
        // remove from the list
        if (cc->prev != NULL) cc->prev->next = cc->next; else termlist = cc->next;
        if (cc->next != NULL) cc->next->prev = cc->prev;
        term_cleanup(cc);
        free(cc);
      }
      // tab numbers changed, so we need to update tabs
      force_frame_redraw(1);
      force_tab_redraw();
    }
  }
}


//==========================================================================
//
//  fix_term_size
//
//==========================================================================
static void fix_term_size (Term *term) {
  if (term != NULL) {
    if (winch_allowed_time == 0) {
      winch_allowed_time = yterm_get_msecs() + max2(0, opt_winch_delay);
    }
    const int neww = clamp(winWidth / charWidth, 1, MaxBufferWidth);
    const int newh = clamp(winHeight / charHeight, 1, MaxBufferHeight);
    if (neww != TERM_CBUF(term)->width || newh != TERM_CBUF(term)->height) {
      if (opt_winch_delay < 1 || yterm_get_msecs() >= winch_allowed_time) {
        #if 0
        fprintf(stderr, "WINCH fix.\n");
        #endif
        if (!opt_vttest && term_fix_size(term)) {
          // redraw immediately
          force_frame_redraw(1);
        }
      } else {
        #if 0
        fprintf(stderr, "WINCH delay...\n");
        #endif
      }
    }
  }
}


//==========================================================================
//
//  term_activate
//
//==========================================================================
static void term_activate (Term *term) {
  if (term != NULL && term->deadstate != DS_DEAD && currterm != term) {
    if (currterm != NULL) {
      yterm_assert(currterm->active != 0);
      if (winFocused) {
        term_invalidate_cursor(currterm);
        term_send_focus_event(currterm, 0);
      }
      //currterm->history.blocktype = SBLOCK_NONE;
      currterm->active = 0;
    }

    currterm = term;
    term->active = 1;
    force_frame_redraw(1);
    term->title.next_check_time = 0;
    if (winFocused) term_send_focus_event(term, 1);
    force_tab_redraw();

    if (TERM_IN_SELECTION(term)) {
      cbuf_free(&hvbuf);
      cbuf_new(&hvbuf, TERM_CBUF(term)->width, TERM_CBUF(term)->height);
      memcpy(hvbuf.buf, TERM_CBUF(term)->buf,
             (unsigned)(TERM_CBUF(term)->width * TERM_CBUF(term)->height) * sizeof(CharCell));
      hvbuf.dirtyCount = TERM_CBUF(term)->dirtyCount;
      term_draw_selection(term);
    }

    fix_term_size(term);
  }
}


//==========================================================================
//
//  find_active_terminal
//
//  returns non-zero if we need to refresh terminal
//
//==========================================================================
static void find_active_terminal (void) {
  // if we have no active terminal, find a new one
  Term *term = currterm;

  if (term == NULL) {
    Term *term = termlist;
    if (term == NULL) return; // no more
    while (term != NULL) {
      // select most recently used tab
      if (term->deadstate != DS_DEAD &&
          (currterm == NULL || term->last_acttime > currterm->last_acttime))
      {
        currterm = term;
      }
      term = term->next;
    }

    // and properly activate it
    term = currterm; currterm = NULL;
    term_activate(term);
    yterm_assert(currterm != NULL);
    yterm_assert(currterm == term);
  } else {
    fix_term_size(term);
  }
}


//==========================================================================
//
//  check_draw_frame
//
//==========================================================================
static void check_draw_frame (void) {
  Term *term = currterm;
  if (term == NULL) return;

  const uint64_t ctt = yterm_get_msecs();
  term->last_acttime = ctt;

  // check terminal titles
  if (winMapped) {
    Term *tt = termlist;
    while (tt != NULL) {
      if (tt->deadstate == DS_ALIVE && tt->child.cmdfd >= 0) {
        if (ctt >= tt->title.next_check_time) {
          tt->title.next_check_time = ctt + 600 + rand()%200;
          // check if process changed
          if (tt->title.custom) {
            pid_t pgrp = tcgetpgrp(tt->child.cmdfd);
            if (pgrp > 0 && pgrp != tt->title.pgrp) tt->title.custom = 0;
          }
          if (!tt->title.custom) {
            term_check_title(tt);
            if (tt->active) x11_change_title(tt->title.last);
            force_tab_redraw();
          }
        }
      }
      tt = tt->next;
    }
  }

  // filter out OSDs
  Message *osd = term->osd_messages;
  if (osd != NULL) {
    while (osd != NULL && osd->time <= ctt) {
      // invalidate top line (that's where OSDs are)
      cbuf_mark_line_dirty(TERM_CBUF(term), 0);
      free(osd->message);
      osd = osd->next;
      free(term->osd_messages);
      term->osd_messages = osd;
      if (osd != NULL) osd->time += ctt;
    }
  }

  // render screen
  if (next_redraw_time <= ctt) {
    if (winVisible) {
      if (term_scroll_locked(term)) {
        // everything we need to draw is properly marked as dirty, do nothing
      } else if (term->scroll_count < 0) {
        // scroll up
        const int lines = -term->scroll_count;
        if (lines >= term->scroll_y1 - term->scroll_y0) {
          // fully scrolled away, full repaint
          cbuf_mark_region_dirty(TERM_CBUF(term),
                                 0, term->scroll_y0,
                                 TERM_CBUF(term)->width,
                                 term->scroll_y1);
        } else {
          #if 0
          fprintf(stderr, "SCROLL-ACC-UP: y0=%d; y1=%d; lines=%d\n",
                           term->scroll_y0, term->scroll_y1, lines);
          #endif
          const int wwdt = TERM_CBUF(term)->width * charWidth;
          const int winy = term->scroll_y0 * charHeight;
          const int winh = (term->scroll_y1 - term->scroll_y0 + 1 - lines) * charHeight;
          XCopyArea(x11_dpy, (Drawable)x11_win, (Drawable)x11_win, x11_gc,
                    0, winy + lines * charHeight, wwdt, winh, 0, winy);
          // should not happen, but...
          menu_mark_all_dirty(curr_menu);
        }
      } else if (term->scroll_count > 0) {
        // scroll down
        const int lines = term->scroll_count;
        if (lines >= term->scroll_y1 - term->scroll_y0) {
          // fully scrolled away, full repaint
          cbuf_mark_region_dirty(TERM_CBUF(term),
                                 0, term->scroll_y0,
                                 TERM_CBUF(term)->width,
                                 term->scroll_y1);
        } else {
          #if 0
          fprintf(stderr, "SCROLL-ACC-DOWN: y0=%d; y1=%d; lines=%d\n",
                           term->scroll_y0, term->scroll_y1, lines);
          #endif
          const int wwdt = TERM_CBUF(term)->width * charWidth;
          const int winy = term->scroll_y0 * charHeight;
          const int winh = (term->scroll_y1 - term->scroll_y0 + 1 - lines) * charHeight;
          XCopyArea(x11_dpy, (Drawable)x11_win, (Drawable)x11_win, x11_gc,
                    0, winy, wwdt, winh, 0, winy + lines * charHeight);
        }
      }
      term->scroll_count = 0;
      // this can be optimised
      if (TERM_CBUF(term)->dirtyCount != 0) menu_mark_dirty_after_cell_redraw();
      CALL_RENDER(
        renderDirty(rcbuf, rcpos);
      );
    }
    const int fps = clamp(opt_fps, 1, 120);
    next_redraw_time = ctt + 1000/fps;
  }

  // updater will repaint tabs if necessary
  if (need_update_tabs) update_all_tabs();
}


//==========================================================================
//
//  blink_cursor
//
//==========================================================================
static void blink_cursor (void) {
  Term *term = currterm;

  if (TERM_CURVISIBLE(term) && winFocused && winVisible) {
    int newPhase;
    if (opt_cur_blink_time > 0) {
      newPhase = ((int)(yterm_get_msecs() % (unsigned)(opt_cur_blink_time * 2)) >= opt_cur_blink_time);
    } else {
      newPhase = 0;
    }
    if (newPhase != curPhase) {
      #if 0
      fprintf(stderr, "***CURBLINK!***\n");
      #endif
      curPhase = newPhase;
      if (TERM_IN_SELECTION(term)) {
        if (term->history.inprogress) term_draw_selection(term);
        cbuf_mark_dirty(&hvbuf, term->history.cx, term->history.cy);
      } else {
        term_invalidate_cursor(term);
      }
      force_frame_redraw(0); // redraw everything, why not
    }
  }
}


//==========================================================================
//
//  has_term_to_write
//
//==========================================================================
static yterm_bool has_term_to_write (void) {
  yterm_bool res = 0;
  Term *term = termlist;
  while (res == 0 && term != NULL) {
    res = (term->wrbuf.used != 0);
    term = term->next;
  }
  return res;
}


//==========================================================================
//
//  wait_for_events
//
//==========================================================================
static void wait_for_events (int xfd) {
  Term *term = currterm;

  XFlush(x11_dpy);
  while (!XPending(x11_dpy) && !has_term_to_write()) {
    int tout;
    if (!winVisible) {
      // invisible window, wait infinitely (if we have nothing to write)
      tout = -1;
    } else {
      const uint64_t ctt = yterm_get_msecs();
      if (next_redraw_time <= ctt) {
        // poll, for some reason
        tout = 0;
      } else if (!HAS_DIRTY(term)) {
        // no dirty cells, wait until cursor blink
        if (winFocused && TERM_CURVISIBLE(term) && opt_cur_blink_time > 0) {
          tout = opt_cur_blink_time - (int)(ctt % (unsigned)opt_cur_blink_time);
        } else {
          // cursor is invisible, wait infinitely
          tout = -1;
        }
      } else {
        // has dirty cells, wait until a new frame
        tout = (int)(next_redraw_time - ctt);
      }
    }
    // debug slowdown is handled at a different place
    #ifdef USELESS_DEBUG_FEATURES
    if (opt_debug_slowdown) tout = 0;
    #endif
    // waiting for the "first run"?
    if (first_run_xtime != 0) {
      const uint64_t ctt = yterm_get_msecs();
      if (first_run_xtime <= ctt) tout = 0;
      else if (tout < 0 || tout > (int)(first_run_xtime - ctt)) tout = (int)(first_run_xtime - ctt);
    }
    #ifdef DUMP_RW_TIMEOUTS
    fprintf(stderr, "TOUT: %d\n", tout);
    #endif
    fd_set rfd;
    fd_set wfd;
    struct timeval timeout;
    if (tout > 0) {
      timeout.tv_sec = tout / 1000;
      timeout.tv_usec = tout * 1000;
    } else {
      memset(&timeout, 0, sizeof(timeout));
    }
    FD_ZERO(&rfd);
    FD_ZERO(&wfd);
    // add X11 socket
    FD_SET(xfd, &rfd);
    int maxfd = xfd;
    // add signal fd
    FD_SET(childsigfd, &rfd);
    maxfd = max2(maxfd, childsigfd);
    // add terminal reading and writing fds
    Term *term = termlist;
    while (term != NULL) {
      if (term->deadstate == DS_ALIVE && term->child.cmdfd >= 0) {
        FD_SET(term->child.cmdfd, &rfd);
        maxfd = max2(maxfd, term->child.cmdfd);
        // if we need to write something, mark this too
        if (term->wrbuf.used != 0) {
          FD_SET(term->child.cmdfd, &wfd);
        }
      }
      term = term->next;
    }
    if (select(maxfd + 1, &rfd, &wfd, NULL, (tout >= 0 ? &timeout : NULL)) < 0) {
      if (errno == EINTR) continue;
      fprintf(stderr, "FATAL: select failed: %s\n", strerror(errno));
      exit(1);
    }
    break;
  }
}


//==========================================================================
//
//  term_create_new_term
//
//==========================================================================
static Term *term_create_new_term (void) {
  Term *term = calloc(1, sizeof(Term));
  if (term != NULL) {
    term_init(term, max2(1, winWidth / charWidth),
                    max2(1, winHeight / charHeight));
    Term *last = termlist;
    if (last != NULL) {
      while (last->next != NULL) last = last->next;
      term->prev = last;
      last->next = term;
    } else {
      termlist = term;
    }
  }
  return term;
}


//==========================================================================
//
//  x11_main_loop
//
//  event processing loop
//
//==========================================================================
static void x11_main_loop (void) {
  yterm_bool quit = 0;
  XEvent x11eventrec;
  Term *term;

  const int xfd = XConnectionNumber(x11_dpy);
  yterm_assert(xfd >= 0);

  while (!quit && x11_win != None && !opt_debug_perform_hotswap) {
    remove_dead_terminals();

    find_active_terminal();
    if (currterm == NULL) break; // no more terminals

    term = currterm;

    // fix mouse events
    // it is safe to call `x11_enable_mouse()` repeatedly,
    // because it remembers the last state
    if (term_mouse_reports_enabled(term)) {
      x11_enable_mouse((term->mode & YTERM_MODE_MOUSEBTN),
                       (term->mode & YTERM_MODE_MOUSEMOTION));
    } else {
      x11_enable_mouse(0, 0);
    }

    // draw new frame
    check_draw_frame();

    #ifdef DUMP_RW_TIMEOUTS
    fprintf(stderr, "NTT: %llu; ctt: %llu\n", next_redraw_time, yterm_get_msecs());
    #endif

    // write data to terminals while we can
    // there should not be a lot of data, so write everything
    all_term_write();
    wait_for_events(xfd);
    collect_dead_children(childsigfd);
    all_term_read(next_redraw_time);

    while (XPending(x11_dpy)) {
      // process X11 events
      XNextEvent(x11_dpy, &x11eventrec);
      if (!x11_filter_event(&x11eventrec)) {
        if (x11eventrec.xany.window == x11_win) {
          x11_event_process(&x11eventrec);
        }
      }
    }

    blink_cursor();

    if (opt_debug_perform_hotswap) {
      opt_debug_perform_hotswap = 0;
      exec_hotswap();
      open_menu(menu_new_message_box("ERROR!", "Hotswapping failed!"));
    }

    // waiting for the "first run"?
    if (first_run_xtime != 0) {
      const uint64_t ctt = yterm_get_msecs();
      if (first_run_xtime <= ctt) {
        first_run_xtime = 0;
        // first run commandline
        if (first_run_ed.argc != 0 && term != NULL) {
          const yterm_bool rres = term_exec(term, &first_run_ed);
          #if 0
          fprintf(stderr, "FIRST-RUN: <%s> (fd=%d; pid=%u)\n", first_run_ed.argv[0],
                  term->child.cmdfd, (unsigned)term->child.pid);
          #endif
          execsh_free(&first_run_ed);
          if (rres == 0) {
            fprintf(stderr, "FATAL: cannot run initial program!\n");
            quitMessage = 666;
          }
        }
      }
    }

    if (quitMessage) {
      if (quitMessage == 666) quit = 1;
      else { quitMessage = 0; show_quit_prompt(); }
    }
  }

  // shutdown all terminals
  while (termlist != NULL) {
    term_kill_child(termlist);
    termlist = termlist->next;
  }

  if (x11_xic) XDestroyIC(x11_xic);
  if (x11_xim) XCloseIM(x11_xim);
  XFreeGC(x11_dpy, x11_gc);
  if (x11_font.fst) XFreeFont(x11_dpy, x11_font.fst);
  x11_font.fst = NULL;

  if (x11_win != None) {
    if (winMapped) XUnmapWindow(x11_dpy, x11_win);
    XDestroyWindow(x11_dpy, x11_win);
    x11_win = None;
    XSync(x11_dpy, 0);
  }
}


//==========================================================================
//
//  summonChildSaviour
//
//==========================================================================
static int summonChildSaviour (void) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGHUP); // reload config
  sigprocmask(SIG_BLOCK, &mask, NULL); // we block the signal
  //pthread_sigmask(SIG_BLOCK, &mask, NULL); // we block the signal
  return signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, char **argv) {
  if (argc > 1 && strcmp(argv[1], "xsel") == 0) {
    for (int f = 1; f < argc - 1; f += 1) argv[f] = argv[f + 1];
    argc -= 1;
    argv[argc] = NULL;
    return xsel_main(argc, argv);
  }

  memset(&first_run_ed, 0, sizeof(first_run_ed));
  get_exe_path();
  xrm_app_name = strdup("k8-yterm");

  //opt_term = strdup("xterm");
  opt_term = strdup("rxvt");
  opt_mono_font = strdup(DEFAULT_MONO_FONT);
  opt_tabs_font = strdup(DEFAULT_TABS_FONT);
  opt_class = strdup("k8_yterm_x11");
  opt_title = strdup("YTERM");

  opt_paste_from[0] = strdup("$SELF xsel -p -o -t 16666 --trim");
  opt_paste_from[1] = strdup("$SELF xsel -s -o -t 16666 --trim");
  opt_paste_from[2] = strdup("$SELF xsel -b -o -t 16666 --trim");

  opt_copy_to[0] = strdup("$SELF xsel -p -i -t 0 --trim");
  opt_copy_to[1] = strdup("$SELF xsel -s -i -t 0 --trim");
  opt_copy_to[2] = strdup("$SELF xsel -b -i -t 0 --trim");

  cbuf_init_palette();

  termlist = NULL;
  currterm = NULL;
  memset(&hvbuf, 0, sizeof(hvbuf));

  if (execsh_prepare(&first_run_ed, "$SHELL -i") == 0) {
    fprintf(stderr, "WTF?!\n");
    exit(1);
  }

  parse_args(argc, argv);
  // override some initial options
  if (opt_debug_hotswap_fd >= 0) {
    hotswap_load_options();
    winch_allowed_time = 0;
  } else {
    winch_allowed_time = yterm_get_msecs();
  }
  x11_init_display();
  xrm_load_options();

  childsigfd = summonChildSaviour();
  if (childsigfd < 0) {
    fprintf(stderr, "FATAL: can't summon dead children saviour!\n");
    exit(1);
  }

  x11_create_cwin_x11();

  if (opt_debug_hotswap_fd >= 0) {
    execsh_free(&first_run_ed);
    hotswap_load_tabs();
  } else {
    // it will be automatically activated
    if (term_create_new_term() == NULL) {
      fprintf(stderr, "FATAL: cannot create terminal!\n");
      exit(1);
    }
  }
  xrm_unseal_ptr_options();

  #if 0
  fprintf(stderr, "TERM INITIAL: %dx%d\n", TERM_CBUF(term)->width, TERM_CBUF(term)->height);
  #endif

  if (logfname != NULL) {
    opt_dump_fd = open(logfname, O_CLOEXEC|O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (opt_dump_fd < 0) {
      fprintf(stderr, "ERROR: cannot create log file '%s'!\n", logfname);
    } else if (opt_dump_fd_enabled) {
      fprintf(stderr, "ERROR: logging into file '%s'.\n", logfname);
    } else {
      fprintf(stderr, "ERROR: created log file '%s'.\n", logfname);
    }
  }

  x11_main_loop();

  if (opt_dump_fd >= 0) close(opt_dump_fd);

  x11_free_fonts();
  XCloseDisplay(x11_dpy);

  return 0;
}
