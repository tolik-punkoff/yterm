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
  run child, setup terminal for it.
  included directly into the main file.
*/

//#define YTERM_DEBUG_EXEC
//#define YTERM_DUMP_DEAD_CHILDREN


//==========================================================================
//
//  execsh_free
//
//==========================================================================
static void execsh_free (ExecData *ed) {
  if (!ed) return;
  for (int f = 0; f < ed->argc; f += 1) free(ed->argv[f]);
  free(ed->argv);
  free(ed->path);
  memset(ed, 0, sizeof(*ed));
}


//==========================================================================
//
//  is_space
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool is_space (char ch) {
  return (ch > 0 && ch <= 0x20);
}


//==========================================================================
//
//  set_term_envs
//
//==========================================================================
static void set_term_envs (Term *term) {
  (void)term;
  #if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
  setenv("TERM", opt_term, 1);
  setenv("COLORTERM", "truecolor", 1); // advertise RGB support
  #if 1
  char buf[64];
  snprintf(buf, sizeof(buf), "%d", term->wkscr->cbuf.width);
  setenv("COLUMNS", buf, 1);
  snprintf(buf, sizeof(buf), "%d", term->wkscr->cbuf.height);
  setenv("ROWS", buf, 1);
  #else
  unsetenv("COLUMNS");
  unsetenv("ROWS");
  #endif
  #endif
}


//==========================================================================
//
//  execsh_prepare
//
//  zero `ed`, parse command line, allocate `ed` arrays
//  on error, all allocated memory is freed, and `ed` is zeroed
//  if `empty_shell` is not zero, empty or NULL `str` will be
//  replaced with $SHELL or "/bin/sh"
//
//==========================================================================
static yterm_bool execsh_prepare (ExecData *ed, const char *str) {
  if (ed == NULL) return 0;

  #ifdef YTERM_DEBUG_EXEC
  FILE *fo = stderr; //fopen("/tmp/z_k8sterm_exec_debug.log", "w");
  #endif

  memset(ed, 0, sizeof(*ed));
  if (str == NULL || !str[0]) return 0;

  #ifdef YTERM_DEBUG_EXEC
  if (fo) fprintf(fo, "EXECSH: executing `str`: <%s>\n", str);
  #endif

  int argc = 0;
  char **argv = calloc(1024, sizeof(char *));
  if (argv == NULL) return 0;
  while (*str && argc != 1023) {
    while (is_space(*str)) str += 1;
    if (str[0]) {
      const char *b = str;
      while (*str && !is_space(*str)) {
        if (*str == '\\' && str[1]) str += 1;
        str += 1;
      }
      argv[argc] = calloc(1, (uintptr_t)(str - b + 1));
      if (argv[argc] == NULL) {
        for (int f = 0; f < argc; ++f) free(argv[f]);
        free(argv);
        return 0;
      }
      memcpy(argv[argc], b, (uintptr_t)(str - b));
      #ifdef YTERM_DEBUG_EXEC
      if (fo) fprintf(fo, "EXECSH:   argc=%d; argv=<%s>\n", argc, argv[argc]);
      #endif
      argc += 1;
    }
  }
  yterm_assert(argc >= 0 && argc <= 1023);
  argv[argc] = NULL;

  #ifdef YTERM_DEBUG_EXEC
  if (fo && fo != stdout && fo != stderr) fclose(fo);
  #endif

  ed->argv = argv;
  ed->argc = argc;
  if (argc < 1) { execsh_free(ed); return 0; }

  // specials
  const char *exe = ed->argv[0]; // it is always there
  if (exe[0] == '$') {
    if (strcmp(exe, "$SHELL") == 0) {
      char *envshell = getenv("SHELL");
      if (envshell == NULL) envshell = "/bin/sh";
      free(ed->argv[0]);
      ed->argv[0] = strdup(envshell);
      if (ed->argv[0] == NULL) { execsh_free(ed); return 0; }
    } else if (strcmp(exe, "$SELF") == 0) {
      free(ed->argv[0]);
      ed->argv[0] = strdup(exe_path);
      if (ed->argv[0] == NULL) { execsh_free(ed); return 0; }
    }
  }

  return 1;
}


//==========================================================================
//
//  execsh
//
//==========================================================================
static __attribute__((noreturn)) void execsh (ExecData *ed) {
  if (!ed || ed->argc <= 0) exit(EXIT_FAILURE);
  /*
  {
    extern char **environ;
    FILE *fo = fopen("z.log", "a");
    for (char **e = environ; *e; ++e) {
      fprintf(fo, "[%s]\n", *e);
    }
    fclose(fo);
  }
  */

  // close all other fds
  struct rlimit r;
  getrlimit(RLIMIT_NOFILE, &r);
  for (rlim_t f = 3; f < r.rlim_cur; f += 1) close(f);

  // unblock all signals
  //sigsetmask(0);
  sigset_t sset;
  sigemptyset(&sset);
  sigprocmask(SIG_SETMASK, &sset, NULL);

  if (ed->path != NULL && ed->path[0] != 0) {
    chdir(ed->path);
  }

  execvp(ed->argv[0], ed->argv);
  exit(EXIT_FAILURE);
}


//==========================================================================
//
//  term_exec
//
//==========================================================================
static yterm_bool term_exec (Term *term, ExecData *ed) {
  if (term != NULL && ed != NULL && ed->argc > 0 && ed->argv[0] != NULL) {
    yterm_assert(term->child.pid == 0);
    yterm_assert(term->child.cmdfd == -1);
    yterm_assert(term->deadstate == DS_ALIVE);
    int flags;
    int masterfd = -1;
    set_term_envs(term);
    struct winsize wsz;
    struct winsize *wszptr = &wsz;;
    wsz.ws_row = TERM_CBUF(term)->height;
    wsz.ws_col = TERM_CBUF(term)->width;
    wsz.ws_xpixel = 0;
    wsz.ws_ypixel = 0;
    if (TERM_CBUF(term)->height < 1 || TERM_CBUF(term)->width < 1) wszptr = NULL;
    term->child.pid = forkpty(&masterfd, NULL, NULL, wszptr);
    switch (term->child.pid) {
      case -1: /* error */
        fprintf(stderr, "ERROR: fork failed!\n");
        if (masterfd != -1) close(masterfd);
        return 0;
      case 0: /* child */
        setsid(); /* create a new process group */
        execsh(ed);
        break;
      default: /* master */
        #if 0
        fprintf(stderr, "EXEC: argc=%d\n", ed->argc);
        for (int f = 0; f < ed->argc; f += 1) {
          fprintf(stderr, "  #%d: <%s>\n", f, ed->argv[f]);
        }
        #endif
        flags = fcntl(masterfd, F_GETFL, NULL);
        if (flags == -1) {
          fprintf(stderr, "OOPS, cannot get master fd flags.\n");
        } else {
          //fprintf(stderr, "MASTER flags: 0%08XH\n", (unsigned)flags);
          flags |= O_NONBLOCK;
          if (fcntl(masterfd, F_SETFL, flags) == -1) {
            fprintf(stderr, "OOPS, cannot set master fd flags.\n");
          }
        }
        flags = fcntl(masterfd, F_GETFD, NULL);
        if (flags != -1) {
          //fprintf(stderr, "MASTER flags2: 0%08XH\n", (unsigned)flags);
          flags |= FD_CLOEXEC;
          if (fcntl(masterfd, F_SETFD, flags) == -1) {
            fprintf(stderr, "OOPS, cannot set master fd (2) flags.\n");
          }
        }
        term->child.cmdfd = masterfd;
        term_send_size_ioctl(term);
        break;
    }
    return 1;
  } else {
    return 0;
  }
}


//==========================================================================
//
//  collect_dead_children
//
//==========================================================================
static void collect_dead_children (int sigfd) {
  struct signalfd_siginfo si;

  for (;;) {
    fd_set rfd;
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    FD_ZERO(&rfd);
    FD_SET(childsigfd, &rfd);
    const int sres = select(childsigfd+ 1, &rfd, NULL, NULL, &timeout);
    if (sres < 0) {
      if (errno == EINTR) continue;
      return;
    } else if (sres == 0) {
      return;
    } else {
      break;
    }
  }

  while (read(sigfd, &si, sizeof(si)) > 0) {
    // ignore errors here
    if (si.ssi_signo == SIGHUP) {
      fprintf(stderr, "MESSAGE: got SIGHUP, reloading config...\n");
      xrm_reloading = 1;
      xrm_load_options();
      if (currterm != NULL) {
        // we may change colors and such
        force_frame_redraw(1);
        force_tab_redraw();
      }
    }
  }

  for (;;) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid <= 0) break; // no more dead children
    #ifdef YTERM_DUMP_DEAD_CHILDREN
    fprintf(stderr, "CHILD with pid %u is dead!\n", (unsigned)pid);
    #endif
    // find terminal with this pid
    Term *term = termlist;
    while (term != NULL) {
      if (term->child.pid == pid) {
        // this terminal should die
        #ifdef YTERM_DUMP_DEAD_CHILDREN
        fprintf(stderr, "TERM with pid %u (fd=%d) is dead!\n",
                (unsigned)term->child.pid, term->child.cmdfd);
        #endif
        //if (xw.paste_term == tt) xw.paste_term = NULL;
        term->child.pid = 0;
        term_kill_child(term);
        if (WIFEXITED(status)) {
          term->deadstate = DS_DEAD;
          //K8T_DATA(tt)->exitcode = WEXITSTATUS(status);
        } else {
          fprintf(stderr, "found murdered child: %d\n", (int)pid);
          term->deadstate = DS_WAIT_KEY; // we have no child, but we are not dead yet

          char xbuffer[256];
          if (WIFSIGNALED(status)) {
            snprintf(xbuffer, sizeof(xbuffer),
                     "Process %u died due to signal %u!",
                     (unsigned)pid, (unsigned)(WTERMSIG(status)));
          } else {
            snprintf(xbuffer, sizeof(xbuffer),
                     "Process %u died due to unknown reason!",
                     (unsigned)pid);
          }

          term_draw_info_window(term, xbuffer);
        }
        term = NULL;
        force_tab_redraw();
      } else {
        term = term->next;
      }
    }
  }
}
