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
  OSD menus.
  included directly into the main file.
*/

enum {
  MENU_LABEL, // empty caption is solid line
  MENU_SUBMENU,
  MENU_BOOL,
  MENU_3BOOL,
  MENU_BITBOOL32,
};

typedef struct {
  XChar2b *text;
  int len;
  int hotpos; // hotkey position
  KeySym hotkey;
} XChar2Str;

// return 1 to close menu
// `sx` and `sy` are coords to open submenu
typedef yterm_bool (*item_after_action) (void *varptr, int sx, int sy);

typedef struct MenuItem_t {
  XChar2Str caption;
  int type;
  void *varptr;
  uint32_t mask;
  item_after_action after_action;
  struct MenuItem_t *next;
} MenuItem;


struct MenuWindow_t {
  MenuItem *items;
  int items_count;
  int x0, y0, width, height;
  int max_width, max_height;
  int top_item;
  int curr_item;
  XChar2Str title;
  yterm_bool dirty;
  void *term; // non-NULL if tab options menu
  // menus can be nested
  MenuWindow *prev;
};


// ////////////////////////////////////////////////////////////////////////// //
static void menu_free_window (MenuWindow *menu);


//==========================================================================
//
//  xchar2_clear
//
//==========================================================================
static void xchar2_clear (XChar2Str *x2) {
  if (x2 != NULL) {
    free(x2->text);
    memset(x2, 0, sizeof(XChar2Str));
  }
}


//==========================================================================
//
//  xchar2_decode_hot
//
//==========================================================================
static KeySym xchar2_decode_hot (char ch) {
  if (ch >= 'A' && ch <= 'Z') return XK_a + (ch - 'A');
  if (ch >= 'a' && ch <= 'z') return XK_a + (ch - 'a');
  if (ch >= '0' && ch <= '9') return XK_0 + (ch - '0');
  return NoSymbol;
}


//==========================================================================
//
//  xchar2_new
//
//  wipes old string contents
//
//==========================================================================
static void xchar2_new (XChar2Str *x2, const char *str, yterm_bool with_hot) {
  if (x2 != NULL) {
    if (str == NULL) str = "";
    memset(x2, 0, sizeof(XChar2Str));
    x2->hotpos = -1; x2->hotkey = NoSymbol;
    uint32_t cp = 0;
    const char *s = str;
    while (*s) {
      cp = yterm_utf8d_consume(cp, *s); s += 1;
      if (yterm_utf8_valid_cp(cp)) x2->len += 1;
    }
    x2->text = malloc(sizeof(x2->text[0]) * (unsigned)x2->len + 1);
    yterm_assert(x2->text != NULL);
    x2->len = 0;
    s = str;
    while (*s) {
      cp = yterm_utf8d_consume(cp, *s); s += 1;
      if (yterm_utf8_valid_cp(cp)) {
        if (!yterm_utf8_printable_cp(cp)) cp = YTERM_UTF8_REPLACEMENT_CP;
        if (with_hot && cp == '&' && xchar2_decode_hot(*s) != NoSymbol) {
          x2->hotpos = x2->len;
          x2->hotkey = xchar2_decode_hot(*s);
          with_hot = 0;
        } else {
          x2->text[x2->len].byte1 = (cp>>8)&0xffU;
          x2->text[x2->len].byte2 = cp&0xffU;
          x2->len += 1;
        }
      }
    }
  }
}


//==========================================================================
//
//  menu_mark_shadow_cells_dirty
//
//  mark shadow region as dirty (because we need to redraw cells under the shadow)
//
//==========================================================================
static void menu_mark_shadow_cells_dirty (MenuWindow *menu) {
  if (menu != NULL && currterm != NULL) {
    if ((opt_osd_menu_shadow_color & 0xff000000U) == 0) {
      // it is better to mark only two stripes, so do it
      const int mxend = menu->x0 + menu->width + 1;
      const int myend = menu->y0 + menu->height + 1;
      const int mxofs = menu->x0 + opt_osd_menu_shadow_xofs / charWidth;
      const int myofs = menu->y0 + opt_osd_menu_shadow_yofs / charHeight;
      const int sxend = mxend + (opt_osd_menu_shadow_xofs + charWidth - 1) / charWidth;
      const int syend = myend + (opt_osd_menu_shadow_yofs + charHeight - 1) / charHeight;
      // right stripe
      cbuf_mark_region_dirty(TERM_CBUF(currterm), mxend, myofs, sxend, myend - 1);
      // bottom stripe
      cbuf_mark_region_dirty(TERM_CBUF(currterm), mxofs, myend, sxend, syend);
    }
  }
}


//==========================================================================
//
//  menu_mark_dirty
//
//  if we are rendering shadows, we need to mark all parent menus as dirty
//
//==========================================================================
static void menu_mark_dirty (MenuWindow *menu) {
  if ((opt_osd_menu_shadow_color & 0xff000000U) == 0) {
    while (menu != NULL) {
      menu->dirty = 1;
      menu = menu->prev;
    }
  } else {
    if (menu != NULL) menu->dirty = 1;
  }
}


//==========================================================================
//
//  menu_mark_all_dirty
//
//==========================================================================
static void menu_mark_all_dirty (MenuWindow *menu) {
  while (menu != NULL) {
    menu->dirty = 1;
    menu = menu->prev;
  }
}


//==========================================================================
//
//  menu_mark_dirty_after_cell_redraw
//
//==========================================================================
static void menu_mark_dirty_after_cell_redraw (void) {
  if ((opt_osd_menu_shadow_color & 0xff000000U) == 0) {
    MenuWindow * menu = curr_menu;
    while (menu != NULL) {
      menu->dirty = 1;
      menu_mark_shadow_cells_dirty(menu);
      menu = menu->prev;
    }
  }
}


//==========================================================================
//
//  open_menu
//
//==========================================================================
static void open_menu (MenuWindow *menu) {
  if (menu != NULL) {
    yterm_assert(menu->prev == NULL);
    menu->prev = curr_menu;
    curr_menu = menu;
    menu_mark_dirty(menu);
  }
}


//==========================================================================
//
//  close_top_menu
//
//==========================================================================
static void close_top_menu (void) {
  MenuWindow *menu = curr_menu;
  if (menu != NULL) {
    curr_menu = menu->prev; menu->prev = NULL;
    menu_mark_shadow_cells_dirty(menu);
    menu_mark_all_dirty(curr_menu);
    menu_free_window(menu);
    force_frame_redraw(1);
  }
}


//==========================================================================
//
//  menu_center
//
//  call after menu creation, or display will not be properly repainted
//
//==========================================================================
static void menu_center (MenuWindow *menu) {
  if (menu != NULL) {
    int nx = (winWidth - menu->width * charWidth) / 2;
    menu->x0 = nx / charWidth;
    int ny = (winHeight - menu->height * charHeight) / 2;
    menu->y0 = ny / charHeight;
  }
}


//==========================================================================
//
//  menu_shift
//
//  call after menu creation, or display will not be properly repainted
//  this can be used to normalize menu position
//
//==========================================================================
static void menu_shift (MenuWindow *menu, int dx, int dy) {
  if (menu != NULL) {
    menu->x0 = clamp(menu->x0 + dx, 0, winWidth / charWidth - menu->width);
    menu->y0 = clamp(menu->y0 + dy, 0, winHeight / charHeight - menu->height);
  }
}


//==========================================================================
//
//  menu_set_position
//
//   call after menu creation, or display will not be properly repainted
//
//==========================================================================
static void menu_set_position (MenuWindow *menu, int x0, int y0) {
  if (menu != NULL) {
    menu->x0 = clamp(x0, 0, winWidth / charWidth - menu->width);
    menu->y0 = clamp(y0, 0, winHeight / charHeight - menu->height);
  }
}


//==========================================================================
//
//  menu_new_window
//
//==========================================================================
static MenuWindow *menu_new_window (int x0, int y0, const char *title,
                                    int max_width, int max_height)
{
  MenuWindow *menu = calloc(1, sizeof(MenuWindow));
  yterm_assert(menu != NULL);
  menu->x0 = x0; menu->y0 = y0;
  if (max_width == 0) max_width = (winWidth / charWidth);
  else if (max_width < 0) max_width += (winWidth / charWidth);
  if (max_height == 0) max_height = (winHeight / charHeight);
  else if (max_height < 0) max_height += (winHeight / charHeight);
  menu->max_width = max2(1, max_width - 5) + 5;
  menu->max_height = max2(1, max_height - 3) + 3;
  //menu->width = 5;
  menu->height = 2;
  xchar2_new(&menu->title, title, 0);
  menu->width = clamp(menu->title.len + 4, 5, max_width);
  menu_mark_dirty(menu);
  return menu;
}


//==========================================================================
//
//  menu_free_window
//
//==========================================================================
static void menu_free_window (MenuWindow *menu) {
  if (menu != NULL) {
    while (menu->items != NULL) {
      MenuItem *it = menu->items; menu->items = it->next;
      xchar2_clear(&it->caption);
      free(it);
    }
    xchar2_clear(&menu->title);
  }
}


//==========================================================================
//
//  menu_add_label_ex
//
//==========================================================================
static void menu_add_label_ex (MenuWindow *menu, const char *caption, int type,
                               item_after_action after_action)
{
  yterm_assert(type == MENU_LABEL || type == MENU_SUBMENU);

  if (menu != NULL) {
    MenuItem *it = calloc(1, sizeof(MenuItem));
    yterm_assert(it != NULL);
    if (type == MENU_LABEL && after_action == NULL && caption == NULL) {
      it->caption.text = NULL;
      it->caption.len = 0;
    } else {
      xchar2_new(&it->caption, caption, (after_action != NULL));
    }
    it->type = type;
    it->varptr = NULL;
    it->after_action = after_action;
    it->next = NULL;

    MenuItem *last = menu->items;
    if (last == NULL) {
      menu->items = it;
    } else {
      while (last->next != NULL) last = last->next;
      last->next = it;
    }

    menu->items_count += 1;

    menu->width = max2(menu->width, it->caption.len + 4 + (type == MENU_SUBMENU ? 2 : 0));
    menu->width = clamp(menu->width, 5, menu->max_width);

    menu->height = clamp(menu->height + 1, 3, menu->max_height);

    menu_mark_dirty(menu);
  }
}


//==========================================================================
//
//  menu_add_label
//
//==========================================================================
static void menu_add_label (MenuWindow *menu, const char *caption,
                            item_after_action after_action)
{
  menu_add_label_ex(menu, caption, MENU_LABEL, after_action);
}


//==========================================================================
//
//  menu_add_submenu
//
//==========================================================================
static void menu_add_submenu (MenuWindow *menu, const char *caption,
                              item_after_action after_action)
{
  menu_add_label_ex(menu, caption, MENU_SUBMENU, after_action);
}


//==========================================================================
//
//  menu_add_var_ex
//
//==========================================================================
static void menu_add_var_ex (MenuWindow *menu, const char *caption, void *varptr, int type,
                             uint32_t mask, item_after_action after_action) {
  if (menu != NULL) {
    //yterm_assert(varptr != NULL);
    yterm_assert(type >= MENU_BOOL && type <= MENU_BITBOOL32);

    MenuItem *it = calloc(1, sizeof(MenuItem));
    yterm_assert(it != NULL);
    xchar2_new(&it->caption, caption, 1);
    it->type = type;
    it->varptr = varptr;
    it->mask = mask;
    it->after_action = after_action;
    it->next = NULL;

    MenuItem *last = menu->items;
    if (last == NULL) {
      menu->items = it;
    } else {
      while (last->next != NULL) last = last->next;
      last->next = it;
    }

    menu->items_count += 1;

    menu->width = max2(menu->width, it->caption.len + 4 + 4);
    menu->width = clamp(menu->width, 5, menu->max_width);

    menu->height = clamp(menu->height + 1, 3, menu->max_height);

    menu_mark_dirty(menu);
  }
}


//==========================================================================
//
//  menu_add_bool
//
//==========================================================================
static void menu_add_bool (MenuWindow *menu, const char *caption, void *varptr,
                           item_after_action after_action)
{
  menu_add_var_ex(menu, caption, varptr, MENU_BOOL, 0, after_action);
}


//==========================================================================
//
//  menu_add_3bool
//
//==========================================================================
static void menu_add_3bool (MenuWindow *menu, const char *caption, void *varptr,
                           item_after_action after_action)
{
  menu_add_var_ex(menu, caption, varptr, MENU_3BOOL, 0, after_action);
}


//==========================================================================
//
//  menu_add_bitbool32
//
//==========================================================================
static void menu_add_bitbool32 (MenuWindow *menu, const char *caption, void *varptr,
                                uint32_t mask, item_after_action after_action)
{
  menu_add_var_ex(menu, caption, varptr, MENU_BITBOOL32, mask, after_action);
}


//==========================================================================
//
//  menu_msgbox_close_cb
//
//==========================================================================
static yterm_bool menu_msgbox_close_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  return 1; // close it
}


//==========================================================================
//
//  menu_new_message_box
//
//==========================================================================
static MenuWindow *menu_new_message_box (const char *title, const char *message) {
  MenuWindow *menu = menu_new_window(0, 0, title, 0, 0);
  if (message != NULL && message[0]) {
    menu_add_label(menu, message, NULL);
    menu_add_label(menu, NULL, NULL);
    menu->curr_item = 2;
  }
  menu_add_label(menu, "&Close", &menu_msgbox_close_cb);
  menu_center(menu);
  return menu;
}


//==========================================================================
//
//  menu_make_cursor_visible
//
//==========================================================================
static void menu_make_cursor_visible (MenuWindow *menu) {
  if (menu != NULL && menu->items_count != 0) {
    const int prev_c = menu->curr_item, prev_t = menu->top_item;
    menu->curr_item = clamp(menu->curr_item, 0, menu->items_count - 1);
    menu->top_item = min2(menu->top_item, menu->curr_item);
    const int top = max2(0, menu->curr_item - (menu->items_count + 1));
    menu->top_item = max2(top, menu->top_item);
    if (prev_c != menu->curr_item || prev_t != menu->top_item) {
      menu_mark_dirty(menu);
    }
  }
}


//==========================================================================
//
//  menu_is_enabled
//
//==========================================================================
YTERM_STATIC_INLINE yterm_bool menu_is_enabled (const MenuItem *it) {
  return (it != NULL && (it->type != MENU_LABEL || it->after_action != NULL));
}


//==========================================================================
//
//  menu_find_enabled_item_at_dir
//
//  use `0` as `dir` to check if item at `pos` is enabled
//  return -1 if not found
//  doesn't check `pos` if `dir` is not zero
//
//==========================================================================
static int menu_find_enabled_item_at_dir (MenuWindow *menu, int pos, int dir) {
  int res = -1;
  if (menu != NULL) {
    int cidx = 0;
    MenuItem *it = menu->items;
    if (dir < 0) {
      while (cidx < pos && it != NULL) {
        if (menu_is_enabled(it)) res = cidx;
        cidx += 1; it = it->next;
      }
    } else if (dir > 0) {
      while (res == -1 && it != NULL) {
        if (cidx > pos && menu_is_enabled(it)) res = cidx;
        cidx += 1; it = it->next;
      }
    } else {
      while (cidx != pos && it != NULL) {
        cidx += 1; it = it->next;
      }
      if (cidx == pos && menu_is_enabled(it)) res = cidx;
    }
  }
  return res;
}


//==========================================================================
//
//  menu_home
//
//==========================================================================
static void menu_home (MenuWindow *menu) {
  if (menu != NULL) {
    const int cidx = menu_find_enabled_item_at_dir(menu, -1, 1);
    if (cidx != -1 && cidx != menu->curr_item) {
      menu->curr_item = cidx;
      menu_mark_dirty(menu);
      menu_make_cursor_visible(menu);
    }
  }
}


//==========================================================================
//
//  menu_end
//
//==========================================================================
static void menu_end (MenuWindow *menu) {
  if (menu != NULL) {
    const int cidx = menu_find_enabled_item_at_dir(menu, menu->items_count, -1);
    if (cidx != -1 && cidx != menu->curr_item) {
      menu->curr_item = cidx;
      menu_mark_dirty(menu);
      menu_make_cursor_visible(menu);
    }
  }
}


//==========================================================================
//
//  menu_up
//
//==========================================================================
static void menu_up (MenuWindow *menu) {
  if (menu != NULL) {
    const int cidx = menu_find_enabled_item_at_dir(menu, menu->curr_item, -1);
    if (cidx != -1 && cidx != menu->curr_item) {
      menu->curr_item = cidx;
      menu_mark_dirty(menu);
      menu_make_cursor_visible(menu);
    }
  }
}


//==========================================================================
//
//  menu_down
//
//==========================================================================
static void menu_down (MenuWindow *menu) {
  if (menu != NULL) {
    const int cidx = menu_find_enabled_item_at_dir(menu, menu->curr_item, 1);
    if (cidx != -1 && cidx != menu->curr_item) {
      menu->curr_item = cidx;
      menu_mark_dirty(menu);
      menu_make_cursor_visible(menu);
    }
  }
}


//==========================================================================
//
//  menu_page_up
//
//==========================================================================
static void menu_page_up (MenuWindow *menu) {
  if (menu != NULL && menu->items_count != 0) {
    int nidx = menu->curr_item;
    if (nidx == menu->top_item) {
      nidx -= menu->height - 2;
    } else {
      nidx = menu->top_item;
    }
    nidx = clamp(nidx, 0, menu->items_count - 1);
    int cidx = menu_find_enabled_item_at_dir(menu, nidx + 1, -1);
    if (cidx == -1) cidx = menu_find_enabled_item_at_dir(menu, nidx - 1, -1);
    if (cidx != -1 && cidx != menu->curr_item) {
      menu->curr_item = cidx;
      menu_mark_dirty(menu);
      menu_make_cursor_visible(menu);
    }
  }
}


//==========================================================================
//
//  menu_page_down
//
//==========================================================================
static void menu_page_down (MenuWindow *menu) {
  if (menu != NULL && menu->items_count != 0) {
    int nidx = menu->curr_item;
    if (nidx == menu->top_item + (menu->height - 2)) {
      nidx += menu->height - 2;
    } else {
      nidx = menu->top_item + (menu->height - 2);
    }
    nidx = clamp(nidx, 0, menu->items_count - 1);
    int cidx = menu_find_enabled_item_at_dir(menu, nidx - 1, 1);
    if (cidx == -1) cidx = menu_find_enabled_item_at_dir(menu, nidx + 1, -1);
    if (cidx != -1 && cidx != menu->curr_item) {
      menu->curr_item = cidx;
      menu_mark_dirty(menu);
      menu_make_cursor_visible(menu);
    }
  }
}


//==========================================================================
//
//  menu_action
//
//  returns non-zero if menu should be closed
//
//==========================================================================
static yterm_bool menu_action (MenuWindow *menu) {
  yterm_bool res = 0;
  if (menu != NULL) {
    MenuItem *it = menu->items;
    int skip = menu->curr_item;
    while (skip != 0 && it != NULL) { skip -= 1; it = it->next; }
    if (it != NULL) {
      int sx = menu->x0 + menu->width - 3;
      int sy = menu->y0 + 1 + (menu->curr_item - menu->top_item);
      switch (it->type) {
        case MENU_LABEL:
        case MENU_SUBMENU:
          menu_mark_dirty(menu);
          if (it->after_action != NULL) res = it->after_action(it->varptr, sx, sy);
          break;
        case MENU_BOOL:
          if (it->varptr != NULL) {
            *(yterm_bool *)it->varptr = !(*(yterm_bool *)it->varptr);
          }
          menu_mark_dirty(menu);
          if (it->after_action != NULL) res = it->after_action(it->varptr, sx, sy);
          break;
        case MENU_3BOOL:
          if (it->varptr != NULL) {
            skip = *(int *)it->varptr;
            skip += 1;
            if (skip > 1) skip = -1;
            *(int *)it->varptr = skip;
          }
          menu_mark_dirty(menu);
          if (it->after_action != NULL) res = it->after_action(it->varptr, sx, sy);
          break;
        case MENU_BITBOOL32:
          if (it->varptr != NULL) {
            uint32_t vv = *(uint32_t *)it->varptr;
            vv ^= it->mask;
            *(uint32_t *)it->varptr = vv;
          }
          menu_mark_dirty(menu);
          if (it->after_action != NULL) res = it->after_action(it->varptr, sx, sy);
          break;
      }
    }
  }
  return res;
}
