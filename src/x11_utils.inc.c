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
  low-level X11 utilities.
  included directly into the main file.
*/

static yterm_bool x11_last_urgency_state = 0;


//==========================================================================
//
//  x11_set_urgency
//
//==========================================================================
static __attribute__((unused)) void x11_set_urgency (yterm_bool urgent) {
  if (!!urgent == !!x11_last_urgency_state) return;
  XWMHints *h = XGetWMHints(x11_dpy, x11_win);
  if (urgent) {
    h->flags |= XUrgencyHint;
  } else {
    h->flags ^= XUrgencyHint;
  }
  XSetWMHints(x11_dpy, x11_win, h);
  XFree(h);
  x11_last_urgency_state = !!urgent;
}


// ////////////////////////////////////////////////////////////////////////// //
// system bell
//

#ifdef USE_CRAPBERRA
#include <dlfcn.h>

static int crapb_inited = 0;

static void *ca_hnd = NULL;
static void *ca_ctx = NULL;

typedef int (*ca_context_create_hnd) (void**);
typedef int (*ca_context_play_hnd) (void*, unsigned int, ...);
typedef int (*ca_context_destroy_hnd) (void*);

static ca_context_create_hnd ca_context_create = NULL;
static ca_context_play_hnd ca_context_play = NULL;
static ca_context_destroy_hnd ca_context_destroy = NULL;

static void *ca_sym (void *hnd, const char *sym) {
  void *hsym = dlsym(hnd, sym);
  if (dlerror()) return NULL;
  return hsym;
}


//==========================================================================
//
//  ca_init
//
//==========================================================================
static void ca_init (void) {
  void *hnd = NULL;
  void *ctx = NULL;
  crapb_inited = -1;
  if (!(hnd = dlopen("libcanberra.so", RTLD_LAZY)) &&
      !(hnd = dlopen("libcanberra.so.0", RTLD_LAZY)) &&
      !(hnd = dlopen("libcanberra.so.0.2.5", RTLD_LAZY)))
  {
    return;
  }
  if (!(ca_context_create = ca_sym(hnd, "ca_context_create")) ||
      !(ca_context_play = ca_sym(hnd, "ca_context_play")) ||
      !(ca_context_destroy = ca_sym(hnd, "ca_context_destroy")))
  {
    dlclose(hnd);
    return;
  }
  if (ca_context_create(&ctx) != 0) {
    ca_context_destroy(ctx);
    dlclose(hnd);
    return;
  }
  ca_hnd = hnd;
  ca_ctx = ctx;
  crapb_inited = 1;
}


//==========================================================================
//
//  ca_done
//
//==========================================================================
static void ca_done (void) {
  if (ca_ctx) ca_context_destroy(ca_ctx);
  if (ca_hnd) dlclose(ca_hnd);
  crapb_inited = 0;
}
#endif /* USE_CRAPBERRA */


//==========================================================================
//
//  x11_bell
//
//==========================================================================
static void x11_bell (void) {
  #ifdef USE_CRAPBERRA
  if (crapb_inited == 0) ca_init();
  if (crapb_inited > 0) {
    if (ca_context_play) {
      ca_context_play(ca_ctx, 0, "event.id", "bell", "event.description", "sterm bell", NULL);
    }
  } else {
    XBell(x11_dpy, 100);
  }
  #else
  XBell(x11_dpy, 100);
  #endif
}
