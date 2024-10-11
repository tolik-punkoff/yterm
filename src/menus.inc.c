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
  various OSD control menus.
  included directly from "x11_keypress.inc.c"
*/


//==========================================================================
//
//  term_menu_history_cb
//
//==========================================================================
static yterm_bool term_menu_history_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  if (currterm != NULL) {
    if (currterm->history.enabled == 0) {
      history_close(&currterm->history.file);
      currterm->history.blocktype = SBLOCK_NONE;
      currterm->history.file.width = TERM_CBUF(currterm)->width;
    }
  }
  return 0; // don't close
}


//==========================================================================
//
//  term_menu_color_mode_global_color_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_global_color_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  colorMode = CMODE_NORMAL;
  return 1; // close menu
}


//==========================================================================
//
//  term_menu_color_mode_global_bw_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_global_bw_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  colorMode = CMODE_BW;
  return 1; // close menu
}


//==========================================================================
//
//  term_menu_color_mode_global_green_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_global_green_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  colorMode = CMODE_GREEN;
  return 1; // close menu
}


//==========================================================================
//
//  term_menu_color_mode_global_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_global_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;

  if (currterm == NULL) return 0;

  MenuWindow *menu = menu_new_window(3, 3, "Global Color Mode", -4, -4);
  menu_add_label(menu, "&color", &term_menu_color_mode_global_color_cb);
  menu_add_label(menu, "&mono", &term_menu_color_mode_global_bw_cb);
  menu_add_label(menu, "&green", &term_menu_color_mode_global_green_cb);

  switch (colorMode) {
    case CMODE_NORMAL: menu->curr_item = 0; break;
    case CMODE_BW: menu->curr_item = 1; break;
    case CMODE_GREEN: menu->curr_item = 2; break;
  }

  menu_set_position(menu, sx, sy);
  //menu_shift(menu, 2, 2);

  open_menu(menu);

  return 0;
}


//==========================================================================
//
//  term_menu_color_mode_tab_default_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_tab_default_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  if (currterm != NULL) currterm->colorMode = -1;
  return 1; // close menu
}


//==========================================================================
//
//  term_menu_color_mode_tab_color_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_tab_color_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  if (currterm != NULL) currterm->colorMode = CMODE_NORMAL;
  return 1; // close menu
}


//==========================================================================
//
//  term_menu_color_mode_tab_bw_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_tab_bw_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  if (currterm != NULL) currterm->colorMode = CMODE_BW;
  return 1; // close menu
}


//==========================================================================
//
//  term_menu_color_mode_tab_green_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_tab_green_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  if (currterm != NULL) currterm->colorMode = CMODE_GREEN;
  return 1; // close menu
}


//==========================================================================
//
//  term_menu_color_mode_tab_cb
//
//==========================================================================
static yterm_bool term_menu_color_mode_tab_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;

  if (currterm == NULL) return 0;

  MenuWindow *menu = menu_new_window(3, 3, "Tab Color Mode", -4, -4);
  menu_add_label(menu, "&default", &term_menu_color_mode_tab_default_cb);
  menu_add_label(menu, "&color", &term_menu_color_mode_tab_color_cb);
  menu_add_label(menu, "&mono", &term_menu_color_mode_tab_bw_cb);
  menu_add_label(menu, "&green", &term_menu_color_mode_tab_green_cb);

  switch (currterm->colorMode) {
    case -1: menu->curr_item = 0; break;
    case CMODE_NORMAL: menu->curr_item = 1; break;
    case CMODE_BW: menu->curr_item = 2; break;
    case CMODE_GREEN: menu->curr_item = 3; break;
  }

  menu_set_position(menu, sx, sy);
  //menu_shift(menu, 2, 2);

  menu->term = currterm;

  open_menu(menu);

  return 0;
}


//==========================================================================
//
//  create_global_options_menu
//
//==========================================================================
static void create_global_options_menu (int sx, int sy) {
  MenuWindow *menu = menu_new_window(max2(sx, 0), max2(sy, 0), "Global Options", -2, -2);
  menu_add_bool(menu, "&history", &opt_history_enabled, NULL);
  menu_add_bool(menu, "&mouse reports", &opt_mouse_reports, NULL);
  menu_add_bool(menu, "&esc-esc fix", &opt_esc_twice, NULL);
  menu_add_bool(menu, "c&yclic tabs", &opt_cycle_tabs, NULL);
  menu_add_bool(menu, "Terminus &gfx chars", &opt_terminus_gfx, NULL);
  menu_add_bool(menu, "&vim idiocity workaround", &opt_workaround_vim_idiocity, NULL);
  menu_add_bool(menu, "selection &tint", &opt_amber_tint, NULL);
  menu_add_label(menu, NULL, NULL);
  menu_add_submenu(menu, "&color mode", &term_menu_color_mode_global_cb);

  if (sx <= 0) menu_center(menu);
  menu_shift(menu, 0, 0);
  open_menu(menu);
}


//==========================================================================
//
//  term_menu_tab_global_submenu_cb
//
//==========================================================================
static yterm_bool term_menu_tab_global_submenu_cb (void *varptr, int sx, int sy) {
  (void)varptr;
  create_global_options_menu(sx, sy);
  return 0; // don't close
}


//==========================================================================
//
//  term_menu_send_tab_resize_cb
//
//==========================================================================
static yterm_bool term_menu_send_tab_resize_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  term_send_size_ioctl(currterm);
  return 1;
}


//==========================================================================
//
//  term_menu_tab_submenu_special_cb
//
//==========================================================================
static yterm_bool term_menu_tab_submenu_special_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;

  if (currterm == NULL) return 0;

  MenuWindow *menu = menu_new_window(sx, sy, "Tab Special Events", 0, 0);
  menu_add_label(menu, "Send &resize event", &term_menu_send_tab_resize_cb);

  menu->term = currterm;
  open_menu(menu);

  return 0;
}


//==========================================================================
//
//  hswap_prompt_yes_cb
//
//==========================================================================
static yterm_bool hswap_prompt_yes_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  opt_debug_perform_hotswap = 1;
  return 1; // close the menu
}


//==========================================================================
//
//  hswap_prompt_no_cb
//
//==========================================================================
static yterm_bool hswap_prompt_no_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  opt_debug_perform_hotswap = 0;
  return 1; // close the menu
}


//==========================================================================
//
//  term_menu_hotswap_submenu_cb
//
//==========================================================================
static yterm_bool term_menu_hotswap_submenu_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;

  MenuWindow *menu = menu_new_window(sx, sy, "Perform hotswap?", 0, 0);
  menu_add_label(menu, "&No", &hswap_prompt_no_cb);
  menu_add_label(menu, "&Yes", &hswap_prompt_yes_cb);

  menu_add_label(menu, NULL, NULL);
  menu_add_label(menu, "This will restart the emulator, keeping", NULL);
  menu_add_label(menu, "all opened tabs and programs intact.", NULL);
  menu_add_label(menu, "It can be used to upgrade the running", NULL);
  menu_add_label(menu, "instance without losing your session.", NULL);

  //menu_center(menu);
  menu_shift(menu, 0, 0);
  open_menu(menu);

  return 0; // don't close the menu
}


//==========================================================================
//
//  term_menu_reload_config_cb
//
//==========================================================================
static yterm_bool term_menu_reload_config_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;

  const MenuWindow *orig = curr_menu;

  xrm_reloading = 1;
  xrm_load_options();
  if (currterm != NULL) {
    // we may change colors and such
    force_frame_redraw(1);
    force_tab_redraw();
  }

  // if we have some new menus, it means that there were some errors
  if (curr_menu == orig) {
    open_menu(menu_new_message_box("Notification", "Config reloaded!"));
  }

  return 0; // don't close the menu
}


//==========================================================================
//
//  create_tab_options_menu
//
//==========================================================================
static void create_tab_options_menu (int sx, int sy) {
  if (currterm != NULL) {
    //fprintf(stderr, "creating tab options menu...\n");
    MenuWindow *menu = menu_new_window(max2(sx, 0), max2(sy, 0), "Tab Options", -2, -2);
    menu_add_bool(menu, "cursor hidd&en", &currterm->wkscr->curhidden, NULL);
    menu_add_bool(menu, "&history", &currterm->history.enabled, term_menu_history_cb);
    menu_add_3bool(menu, "&mouse reports", &currterm->mouseReports, NULL);
    menu_add_3bool(menu, "&esc-esc fix", &currterm->escesc, NULL);
    menu_add_bitbool32(menu, "force &UTF-8", &currterm->mode, YTERM_MODE_FORCED_UTF, NULL);
    menu_add_label(menu, NULL, NULL);
    menu_add_submenu(menu, "&color mode", &term_menu_color_mode_tab_cb);
    menu_add_submenu(menu, "&special events", &term_menu_tab_submenu_special_cb);
    menu_add_label(menu, NULL, NULL);
    menu_add_submenu(menu, "&GLOBAL OPTIONS", &term_menu_tab_global_submenu_cb);
    menu_add_label(menu, NULL, NULL);
    menu_add_label(menu, "&Reload Config", &term_menu_reload_config_cb);
    menu_add_label(menu, NULL, NULL);
    menu_add_submenu(menu, "HOTSWAP", &term_menu_hotswap_submenu_cb);

    menu->term = currterm;

    if (sx <= 0) menu_center(menu);
    menu_shift(menu, 0, 0);
    open_menu(menu);
  }
}


//==========================================================================
//
//  qprompt_yes_cb
//
//==========================================================================
static yterm_bool qprompt_yes_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  quitMessage = 666;
  return 1; // close the menu
}


//==========================================================================
//
//  qprompt_no_cb
//
//==========================================================================
static yterm_bool qprompt_no_cb (void *varptr, int sx, int sy) {
  (void)varptr; (void)sx; (void)sy;
  quitMessage = 0; // just in case
  return 1; // close the menu
}


//==========================================================================
//
//  show_quit_prompt
//
//==========================================================================
static void show_quit_prompt (void) {
  if (currterm != NULL) {
    MenuWindow *menu = menu_new_window(0, 0, "Do you really want to quit?", 0, 0);
    menu_add_label(menu, "&Yes", &qprompt_yes_cb);
    menu_add_label(menu, "&No", &qprompt_no_cb);

    menu_center(menu);
    menu_shift(menu, 0, 0);
    open_menu(menu);
  } else {
    quitMessage = 666;
  }
}


//==========================================================================
//
//  x11_menu_keyevent
//
//==========================================================================
static void x11_menu_keyevent (XKeyEvent *e, KeySym ksym, uint32_t kmods) {
  (void)e; (void)ksym; (void)kmods;

  MenuWindow *menu = curr_menu;
  if (menu == NULL) return;

  if (ksym != NoSymbol) {
    int cidx = 0;
    MenuItem *it = menu->items;
    while (it != NULL && it->caption.hotkey != ksym) {
      cidx += 1; it = it->next;
    }
    if (it != NULL) {
      menu->curr_item = cidx;
      menu_make_cursor_visible(menu);
      menu_mark_dirty(menu);
      if (menu_action(menu)) close_top_menu();
      force_frame_redraw(1);
      return;
    }
  }

  if ((e->state&Mod2Mask) == 0) ksym = translate_keypad(ksym);

  switch (ksym) {
    case XK_Return:
    case XK_KP_Enter:
    case XK_space:
      if (menu_action(menu)) close_top_menu();
      force_frame_redraw(1);
      return;

    case XK_Escape:
      close_top_menu();
      return;

    case XK_Up: menu_up(menu); break;
    case XK_Down: menu_down(menu); break;
    case XK_Home: menu_home(menu); break;
    case XK_End: menu_end(menu); break;
    case XK_Page_Up: menu_page_up(menu); break;
    case XK_Page_Down: menu_page_down(menu); break;
  }

  menu = curr_menu;
  if (menu == NULL) {
    force_frame_redraw(1);
  } else {
    while (menu != NULL && menu->dirty == 0) menu = menu->prev;
    if (menu != NULL) force_frame_redraw(1);
  }
}
