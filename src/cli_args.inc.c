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
  command line arguments parsing.
  included directly into the main file.
*/


#define PUSH_BACK(_c)  (*ress)[dpos++] = (_c)
#define DECODE_TUPLE(tuple,bytes) \
  for (tmp = bytes; tmp > 0; tmp--, tuple = (tuple & 0x00ffffff)<<8) \
    PUSH_BACK((char)((tuple >> 24)&0xff))


//==========================================================================
//
//  ascii85Decode
//
//  returns ress length
//
//==========================================================================
static int ascii85Decode (char **ress, const char *srcs) {
  static uint32_t pow85[5] = { 85*85*85*85UL, 85*85*85UL, 85*85UL, 85UL, 1UL };
  const uint8_t *data = (const uint8_t *)srcs;
  int len = strlen(srcs);
  uint32_t tuple = 0;
  int count = 0, c = 0;
  int dpos = 0;
  int start = 0, length = len;
  int tmp;
  if (start < 0) start = 0; else { len -= start; data += start; }
  if (length < 0 || len < length) length = len;
  *ress = (char *)calloc(1, len+1);
  for (int f = length; f > 0; --f, ++data) {
    c = *data;
    if (c <= ' ') continue; // skip blanks
    switch (c) {
      case 'z': // zero tuple
      if (count != 0) {
        //fprintf(stderr, "%s: z inside ascii85 5-tuple\n", file);
        free(*ress);
        *ress = NULL;
        return -1;
      }
      PUSH_BACK('\0');
      PUSH_BACK('\0');
      PUSH_BACK('\0');
      PUSH_BACK('\0');
      break;
    case '~': // '~>': end of sequence
      if (f < 1 || data[1] != '>') { free(*ress); return -2; } // error
      if (count > 0) { f = -1; break; }
      /* fallthrough */
    default:
      if (c < '!' || c > 'u') {
        //fprintf(stderr, "%s: bad character in ascii85 region: %#o\n", file, c);
        free(*ress);
        return -3;
      }
      tuple += ((uint8_t)(c-'!'))*pow85[count++];
      if (count == 5) {
        DECODE_TUPLE(tuple, 4);
        count = 0;
        tuple = 0;
      }
      break;
    }
  }
  // write last (possibly incomplete) tuple
  if (count-- > 0) {
    tuple += pow85[count];
    DECODE_TUPLE(tuple, count);
  }
  return dpos;
}

#undef PUSH_BACK
#undef DECODE_TUPLE


//==========================================================================
//
//  decodeBA
//
//==========================================================================
static void decodeBA (char *str, int len) {
  char pch = 42;
  for (int f = 0; f < len; ++f, ++str) {
    char ch = *str;
    ch = (ch-f-1)^pch;
    *str = ch;
    pch = ch;
  }
}


//==========================================================================
//
//  printEC
//
//==========================================================================
static void printEC (const char *txt) {
  char *dest;
  int len;
  if ((len = ascii85Decode(&dest, txt)) >= 0) {
    decodeBA(dest, len);
    fprintf(stderr, "%s\n", dest);
    free(dest);
  }
}


//==========================================================================
//
//  isStr85Equ
//
//==========================================================================
static yterm_bool isStr85Equ (const char *txt, const char *str) {
  char *dest;
  int len;
  yterm_bool res = 0;
  if (str != NULL && str[0] && (len = ascii85Decode(&dest, txt)) >= 0) {
    if (len > 0) res = (strcmp(dest+1, str+1) == 0);
    free(dest);
  }
  return res;
}


//==========================================================================
//
//  checkEGG
//
//==========================================================================
static yterm_bool checkEGG (const char *str) {
  if (isStr85Equ("06:]JASq", str) || isStr85Equ("0/i", str)) {
    printEC(
      "H8lZV&6)1>+AZ>m)Cf8;A1/cP+CnS)0OJ`X.QVcHA4^cc5r3=m1c%0D3&c263d?EV6@4&>"
      "3DYQo;c-FcO+UJ;MOJ$TAYO@/FI]+B?C.L$>%:oPAmh:4Au)>AAU/H;ZakL2I!*!%J;(AK"
      "NIR#5TXgZ6c'F1%^kml.JW5W8e;ql0V3fQUNfKpng6ppMf&ip-VOX@=jKl;#q\"DJ-_>jG"
      "8#L;nm]!q;7c+hR6p;tVY#J8P$aTTK%c-OT?)<00,+q*8f&ff9a/+sbU,:`<H*[fk0o]7k"
      "^l6nRkngc6Tl2Ngs!!P2I%KHG=7n*an'bsgn>!*8s7TLTC+^\\\"W+<=9^%Ol$1A1eR*Be"
      "gqjEag:M0OnrC4FBY5@QZ&'HYYZ#EHs8t4$5]!22QoJ3`;-&=\\DteO$d6FBqT0E@:iu?N"
      "a5ePUf^_uEEcjTDKfMpX/9]DFL8N-Ee;*8C5'WgbGortZuh1\\N0;/rJB6'(MSmYiS\"6+"
      "<NK)KDV3e+Ad[@).W:%.dd'0h=!QUhghQaNNotIZGrpHr-YfEuUpsKW<^@qlZcdTDA!=?W"
      "Yd+-^`'G8Or)<0-T&CT.i+:mJp(+/M/nLaVb#5$p2jR2<rl7\"XlngcN`mf,[4oK5JLr\\"
      "m=X'(ue;'*1ik&/@T4*=j5t=<&/e/Q+2=((h`>>uN(#>&#i>2/ajK+=eib1coVe3'D)*75"
      "m_h;28^M6p6*D854Jj<C^,Q8Wd\"O<)&L/=C$lUAQNN<=eTD:A6kn-=EItXSss.tAS&!;F"
      "EsgpJTHIYNNnh'`kmX^[`*ELOHGcWbfPOT`J]A8P`=)AS;rYlR$\"-.RG440lK5:Dg?G'2"
      "['dE=nEm1:k,,Se_=%-6Z*L^J[)EC"
    );
    return 1;
  }
  if (isStr85Equ("04Jj?B)", str)) {
    printEC(
      "IPaSa(`c:T,o9Bq3\\)IY++?+!-S9%P0/OkjE&f$l.OmK'Ai2;ZHn[<,6od7^8;)po:HaP"
      "m<'+&DRS:/1L7)IA7?WI$8WKTUB2tXg>Zb$.?\"@AIAu;)6B;2_PB5M?oBPDC.F)606Z$V"
      "=ONd6/5P*LoWKTLQ,d@&;+Ru,\\ESY*rg!l1XrhpJ:\"WKWdOg?l;=RHE:uU9C?aotBqj]"
      "=k8cZ`rp\"ZO=GjkfD#o]Z\\=6^]+Gf&-UFthT*hN"
    );
    return 1;
  }
  if (isStr85Equ("04o69A7Tr", str)) {
    printEC(
      "Ag7d[&R#Ma9GVV5,S(D;De<T_+W).?,%4n+3cK=%4+0VN@6d\")E].np7l?8gF#cWF7SS_m"
      "4@V\\nQ;h!WPD2h#@\\RY&G\\LKL=eTP<V-]U)BN^b.DffHkTPnFcCN4B;]8FCqI!p1@H*_"
      "jHJ<%g']RG*MLqCrbP*XbNL=4D1R[;I(c*<FuesbWmSCF1jTW+rplg;9[S[7eDVl6YsjT"
    );
    return 1;
  }
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  CLI_TTY_SIZE,
  CLI_BLINK,
  CLI_XEMBED,
  CLI_CLR_MODE,
  CLI_TTY_TYPE,
  CLI_FPS,
  //CLI_TABS,
  CLI_BOOL,
  CLI_STR,
  CLI_EXEC,

  CLI_END=-1,
};

typedef struct {
  char sname;
  const char *lname;
  int type;
  void *varptr;
  const char *help;
} CliArg;


static CliArg cliargs[] = {
  {.sname='t', .lname="title", .type=CLI_STR,     .varptr=&opt_title,     .help="default X11 window title"},
  {.sname='c', .lname="class", .type=CLI_STR,     .varptr=&opt_class,     .help="X11 window class"},
  {.sname=0,   .lname="type",  .type=CLI_TTY_TYPE,.varptr=&opt_term_type, .help="terminal type: RXVT/XTERM"},
  {.sname=0,   .lname="term",  .type=CLI_STR,     .varptr=&opt_term,      .help="value for $TERM"},

  {.sname=0, .lname="app", .type=CLI_STR, .varptr=&xrm_app_name, .help="app name for Xresources"},

  {.sname=0, .lname="color",   .type=CLI_CLR_MODE, .varptr=&colorMode, .help="color more: bw/mono, green, color"},

  {.sname=0, .lname="escesc",  .type=CLI_BOOL, .varptr=&opt_esc_twice,       .help="send ESC ESC on escape, and single ESC on alt-escape"},
  {.sname=0, .lname="mreport", .type=CLI_BOOL, .varptr=&opt_mouse_reports,   .help="enable/disable mouse reports"},
  {.sname=0, .lname="tabs",    .type=CLI_BOOL, .varptr=&opt_enable_tabs,     .help="enable/disable tabs"},
  {.sname=0, .lname="history", .type=CLI_BOOL, .varptr=&opt_history_enabled, .help="enable/disable scrollback history"},

  {.sname=0, .lname="into",  .type=CLI_XEMBED, .varptr=&x11_embed, .help="embed into window with the given id"},
  {.sname=0, .lname="embed", .type=CLI_XEMBED, .varptr=&x11_embed, .help="embed into window with the given id"},

  {.sname='b', .lname="blink", .type=CLI_BLINK, .varptr=&opt_cur_blink_time, .help="cursor blink time, msecs (0: disabled)"},

  {.sname='W', .lname="width",  .type=CLI_TTY_SIZE, .varptr=&opt_twidth, .help="initial terminal width"},
  {.sname='H', .lname="height", .type=CLI_TTY_SIZE, .varptr=&opt_theight,.help="initial terminal height"},

  {.sname=0, .lname="fps", .type=CLI_FPS, .varptr=&opt_fps, .help="screen refresh rate, frames per second"},

  {.sname='e', .lname="exec", .type=CLI_EXEC, .varptr=NULL, .help="run command"},

  {.type=CLI_END},
};


static const char *logfname = NULL;

//==========================================================================
//
//  parse_args
//
//==========================================================================
static void parse_args (int argc, char **argv) {
  int f = 1;
  while (f < argc) {
    const char *aname = argv[f]; f += 1;
    if (aname == NULL || aname[0] == 0) continue;
    if (aname[0] == '-' && checkEGG(aname)) exit(1);
    // name (ignored)
    if (strcmp(aname, "-name") == 0 || strcmp(aname, "--name") == 0) continue;
    // -S (hack!)
    if (strcmp(aname, "-S") == 0) {
      if (!xrm_is_option_sealed_by_ptr(&opt_enable_tabs)) {
        xrm_seal_option_by_ptr(&opt_enable_tabs);
        opt_enable_tabs = 0;
      }
      continue;
    }
    // dump-esc (dump escapes)
    if (strcmp(aname, "-dump-esc") == 0 || strcmp(aname, "--dump-esc") == 0) {
      opt_dump_esc_enabled = 1;
      continue;
    }
    // vttest (special mode for vttest)
    if (strcmp(aname, "-vttest") == 0 || strcmp(aname, "--vttest") == 0) {
      opt_vttest = 1;
      continue;
    }
    // vttest (special mode for vttest)
    if (strcmp(aname, "-wait") == 0 || strcmp(aname, "--wait") == 0) {
      opt_wait_last_term = 1;
      continue;
    }
    // others are with argument
    const char *avalue = (f < argc ? argv[f++] : NULL);
    if (avalue != NULL && avalue[0] == 0) avalue = NULL;
    // hotswap
    if (strcmp(aname, HOTSWAP_CLI_ARG) == 0) {
      if (opt_debug_hotswap_fd >= 0 || avalue == NULL) exit(1);
      char *end;
      long v = strtol(avalue, &end, 10);
      if (v < 3 || *end) exit(1);
      opt_debug_hotswap_fd = (int)v;
      continue;
    }
    // log (debug log)
    if (strcmp(aname, "-log") == 0) {
      if (avalue == NULL) {
        fprintf(stderr, "FATAL: log file name?\n");
        exit(1);
      }
      logfname = avalue;
      opt_dump_fd_enabled = 1;
      continue;
    }
    // qlog (debug log, but don't start immediately)
    if (strcmp(aname, "-qlog") == 0) {
      if (avalue == NULL) {
        fprintf(stderr, "FATAL: log file name?\n");
        exit(1);
      }
      logfname = avalue;
      opt_dump_fd_enabled = 0;
      continue;
    }

    if (aname[0] != '-' || aname[1] == '-') {
      fprintf(stderr, "wtf '%s' means?\n", aname);
      exit(1);
    }

    yterm_bool found = 0;
    for (CliArg *nfo = cliargs; !found && nfo->type != CLI_END; nfo += 1) {
      if ((nfo->sname != 0 && aname[1] == nfo->sname && aname[2] == 0) ||
          (nfo->lname != NULL && strcmp(aname + 1, nfo->lname) == 0))
      {
        char *end;
        long lv;
        found = 1;
        if (avalue == NULL) {
          fprintf(stderr, "missing argument for '%s'\n", aname);
          exit(1);
        }
        switch (nfo->type) {
          case CLI_TTY_SIZE:
            lv = strtol(avalue, &end, 10);
            if (lv < 8 || *end || lv > 16384) {
              fprintf(stderr, "ERROR: invalid tty size for '%s'!\n", aname);
              exit(1);
            }
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              *(int *)nfo->varptr = (int)lv;
            }
            break;
          case CLI_BLINK:
            lv = strtol(avalue, &end, 10);
            if (lv < 0 || *end || lv > 6666) {
              fprintf(stderr, "ERROR: invalid cursor blink time for '%s'!\n", aname);
              exit(1);
            }
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              *(int *)nfo->varptr = (int)lv;
            }
            break;
          case CLI_XEMBED:
            lv = strtol(avalue, &end, 10);
            if (lv <= 0 || *end) {
              fprintf(stderr, "ERROR: invalid window id for '%s'!\n", aname);
              exit(1);
            }
            *(Window *)nfo->varptr = (Window)lv;
            /*
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              *(Window *)nfo->varptr = (Window)lv;
            }
            */
            break;
          case CLI_CLR_MODE:
            lv = -1;
            if (strEquCI(avalue, "bw") || strEquCI(avalue, "mono")) lv = CMODE_BW;
            else if (strEquCI(avalue, "green")) lv = CMODE_GREEN;
            else if (strEquCI(avalue, "normal")) lv = CMODE_NORMAL;
            else if (strEquCI(avalue, "color")) lv = CMODE_NORMAL;
            else {
              fprintf(stderr, "ERROR: invalid color mode for '%s'!\n", aname);
              exit(1);
            }
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              *(int *)nfo->varptr = (int)lv;
            }
            break;
          case CLI_TTY_TYPE:
            lv = -1;
            if (strEquCI(avalue, "rxvt")) lv = TT_RXVT;
            else if (strEquCI(avalue, "xterm")) lv = TT_XTERM;
            else {
              fprintf(stderr, "ERROR: invalid terminal type for '%s'!\n", aname);
              exit(1);
            }
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              *(int *)nfo->varptr = (int)lv;
            }
            break;
          case CLI_FPS:
            lv = strtol(avalue, &end, 10);
            if (*end || lv < 1 || lv > 120) {
              fprintf(stderr, "ERROR: invalid FPS value!\n");
              exit(1);
            }
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              *(int *)nfo->varptr = (int)lv;
            }
            break;
          //case CLI_TABS:
          case CLI_BOOL:
            lv = parse_bool(avalue);
            if (lv < 0) {
              fprintf(stderr, "ERROR: invalid boolean for '%s'!\n", aname);
              exit(1);
            }
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              //fprintf(stderr, "cmd: <%s>; avalue=<%s>; lv=%d\n", aname, avalue, !!lv);
              *(yterm_bool *)nfo->varptr = !!lv;
            }
            break;
          case CLI_STR:
            if (!xrm_is_option_sealed_by_ptr(nfo->varptr)) {
              xrm_seal_option_by_ptr(nfo->varptr);
              end = *(char **)nfo->varptr;
              free(end);
              end = strdup(avalue);
              yterm_assert(end != NULL);
              *(char **)nfo->varptr = end;
            }
            break;
          case CLI_EXEC:
            if (!xrm_is_option_sealed_by_ptr(&first_run_ed)) {
              xrm_seal_option_by_ptr(&first_run_ed);
              execsh_free(&first_run_ed);
              f -= 1;
              int left = argc - f;
              if (left < 1) {
                first_run_ed.argc = 1;
                first_run_ed.argv = malloc(2 * sizeof(first_run_ed.argv[0]));
                yterm_assert(first_run_ed.argv != NULL);
                first_run_ed.argv[0] = strdup("$SHELL");
                yterm_assert(first_run_ed.argv[0] != NULL);
                first_run_ed.argv[1] = NULL;
              } else {
                first_run_ed.argc = left;
                first_run_ed.argv = malloc((unsigned)(left + 1) * sizeof(first_run_ed.argv[0]));
                yterm_assert(first_run_ed.argv != NULL);
                for (int n = 0; n < left; n += 1) {
                  first_run_ed.argv[n] = strdup(argv[f + n]);
                  yterm_assert(first_run_ed.argv[n] != NULL);
                }
                first_run_ed.argv[left] = NULL;
              }
            }
            return;
          default:
            fprintf(stderr, "ketmar forgot to handle some cli arg type!\n");
            abort();
        }
      }
    }
    if (!found) {
      // unknown option
      printf("yterm: minimalistic terminal emulator, by Ketmar Dark\n" \
             "usage: yterm [options]\n" \
             "options:\n");
      char optbuf[1024];
      size_t maxlen = 0;
      for (int pass = 0; pass < 2; pass += 1) {
        for (CliArg *nfo = cliargs; nfo->type != CLI_END; nfo += 1) {
          if (nfo->help != NULL) {
            if (nfo->sname && nfo->lname) {
              snprintf(optbuf, sizeof(optbuf), "-%c -%s", nfo->sname, nfo->lname);
            } else if (nfo->sname) {
              snprintf(optbuf, sizeof(optbuf), "-%c", nfo->sname);
            } else {
              snprintf(optbuf, sizeof(optbuf), "   -%s", nfo->lname);
            }
            switch (nfo->type) {
              case CLI_TTY_SIZE: strcat(optbuf, " <int>"); break;
              case CLI_BLINK: strcat(optbuf, " <int>"); break;
              case CLI_XEMBED: strcat(optbuf, " <wid>"); break;
              case CLI_CLR_MODE: strcat(optbuf, " <mode>"); break;
              case CLI_TTY_TYPE: strcat(optbuf, " <type>"); break;
              //case CLI_TABS: strcat(optbuf, " <count>"); break;
              case CLI_BOOL: strcat(optbuf, " <bool>"); break;
              case CLI_FPS: strcat(optbuf, " <int>"); break;
              case CLI_STR: strcat(optbuf, " <str>"); break;
              case CLI_EXEC: strcat(optbuf, " cmd..."); break;
            }
            size_t blen = strlen(optbuf);
            if (pass == 0) {
              if (maxlen < blen) maxlen = blen;
            } else {
              printf("  %s", optbuf);
              while (blen < maxlen) { printf(" "); blen += 1; }
              printf(" -- %s\n", nfo->help);
            }
          }
        }
      }
      exit(EXIT_FAILURE);
    }
  }
}
