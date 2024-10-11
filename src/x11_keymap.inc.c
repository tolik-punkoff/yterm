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
  included directly from "x11_keypress.inc.c".
*/


/*
n;2 -- Shift
n;3 -- Alt
n;4 -- Alt+Shift
n;5 -- Ctrl
n;6 -- Ctrl+Shift
n;7 -- Ctrl+Alt
n;8 -- Ctrl+Alt+Shift

-1:
001 -- S
010 -- M
011 -- M-S
100 -- C
101 -- C-S
110 -- C-M
111 -- C-M-S
*/

// WARNING! do not put KP keysyms here!
static const KeyTrans keymap[] = {
  {.ksym=XK_BackSpace, .text="\x7f",    .type=TT_ANY},

  // rxvt
  {.ksym=XK_Insert,    .text="\x1b[2~", .type=TT_RXVT},
  {.ksym=XK_Delete,    .text="\x1b[3~", .type=TT_RXVT},
  {.ksym=XK_Prior,     .text="\x1b[5~", .type=TT_RXVT},
  {.ksym=XK_Next,      .text="\x1b[6~", .type=TT_RXVT},
  {.ksym=XK_Home,      .text="\x1b[7~", .type=TT_RXVT},
  {.ksym=XK_End,       .text="\x1b[8~", .type=TT_RXVT},

  {.mods=Mod_Ctrl, .ksym=XK_Insert, .text="\x1b[2^", .type=TT_RXVT},
  {.mods=Mod_Ctrl, .ksym=XK_Delete, .text="\x1b[3^", .type=TT_RXVT},
  {.mods=Mod_Ctrl, .ksym=XK_Prior,  .text="\x1b[5^", .type=TT_RXVT},
  {.mods=Mod_Ctrl, .ksym=XK_Next,   .text="\x1b[6^", .type=TT_RXVT},
  {.mods=Mod_Ctrl, .ksym=XK_Home,   .text="\x1b[7^", .type=TT_RXVT},
  {.mods=Mod_Ctrl, .ksym=XK_End,    .text="\x1b[8^", .type=TT_RXVT},

  {.mods=Mod_Shift, .ksym=XK_Insert, .text="\x1b[2$", .type=TT_RXVT},
  {.mods=Mod_Shift, .ksym=XK_Delete, .text="\x1b[3$", .type=TT_RXVT},
  {.mods=Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5$", .type=TT_RXVT},
  {.mods=Mod_Shift, .ksym=XK_Next,   .text="\x1b[6$", .type=TT_RXVT},
  {.mods=Mod_Shift, .ksym=XK_Home,   .text="\x1b[7$", .type=TT_RXVT},
  {.mods=Mod_Shift, .ksym=XK_End,    .text="\x1b[8$", .type=TT_RXVT},

  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Insert, .text="\x1b[2@", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Delete, .text="\x1b[3@", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5@", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Next,   .text="\x1b[6@", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Home,   .text="\x1b[7@", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_End,    .text="\x1b[8@", .type=TT_RXVT},

  {.mods=Mod_Alt, .ksym=XK_Insert, .text="\x1b[2;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_Delete, .text="\x1b[3;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_Prior,  .text="\x1b[5;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_Next,   .text="\x1b[6;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_Home,   .text="\x1b[7;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_End,    .text="\x1b[8;3~", .type=TT_RXVT},

  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Insert, .text="\x1b[2;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Delete, .text="\x1b[3;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Next,   .text="\x1b[6;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Home,   .text="\x1b[7;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_End,    .text="\x1b[8;4~", .type=TT_RXVT},

  {.mods=Mod_Alt|Mod_Ctrl, .ksym=XK_Insert, .text="\x1b[2;7~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl, .ksym=XK_Delete, .text="\x1b[3;7~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl, .ksym=XK_Prior,  .text="\x1b[5;7~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl, .ksym=XK_Next,   .text="\x1b[6;7~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl, .ksym=XK_Home,   .text="\x1b[7;7~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl, .ksym=XK_End,    .text="\x1b[8;7~", .type=TT_RXVT},

  {.mods=Mod_Alt|Mod_Ctrl|Mod_Shift, .ksym=XK_Insert, .text="\x1b[2;8~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl|Mod_Shift, .ksym=XK_Delete, .text="\x1b[3;8~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl|Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5;8~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl|Mod_Shift, .ksym=XK_Next,   .text="\x1b[6;8~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl|Mod_Shift, .ksym=XK_Home,   .text="\x1b[7;8~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Ctrl|Mod_Shift, .ksym=XK_End,    .text="\x1b[8;8~", .type=TT_RXVT},

  // xterm
  {.ksym=XK_F1, .text="\x1bOP", .type=TT_XTERM},
  {.ksym=XK_F2, .text="\x1bOQ", .type=TT_XTERM},
  {.ksym=XK_F3, .text="\x1bOR", .type=TT_XTERM},
  {.ksym=XK_F4, .text="\x1bOS", .type=TT_XTERM},
  // rxvt
  {.ksym=XK_F1,  .text="\x1b[11~", .type=TT_RXVT},
  {.ksym=XK_F2,  .text="\x1b[12~", .type=TT_RXVT},
  {.ksym=XK_F3,  .text="\x1b[13~", .type=TT_RXVT},
  {.ksym=XK_F4,  .text="\x1b[14~", .type=TT_RXVT},

  {.ksym=XK_F5,  .text="\x1b[15~", .type=TT_ANY},
  {.ksym=XK_F6,  .text="\x1b[17~", .type=TT_ANY},
  {.ksym=XK_F7,  .text="\x1b[18~", .type=TT_ANY},
  {.ksym=XK_F8,  .text="\x1b[19~", .type=TT_ANY},
  {.ksym=XK_F9,  .text="\x1b[20~", .type=TT_ANY},
  {.ksym=XK_F10, .text="\x1b[21~", .type=TT_ANY},
  //{.ksym=XK_F11, .text="\x1b[23~", .type=TT_ANY},
  //{.ksym=XK_F12, .text="\x1b[24~", .type=TT_ANY},

  {.mods=Mod_Ctrl, .ksym=XK_F1,   .text="\x1b[11^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F2,   .text="\x1b[12^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F3,   .text="\x1b[13^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F4,   .text="\x1b[14^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F5,   .text="\x1b[15^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F6,   .text="\x1b[17^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F7,   .text="\x1b[18^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F8,   .text="\x1b[19^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F9,   .text="\x1b[20^", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_F10,  .text="\x1b[21^", .type=TT_ANY},

  // xterm
  {.mods=Mod_Alt, .ksym=XK_F1,    .text="\x1b[1;3P", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_F2,    .text="\x1b[1;3Q", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_F3,    .text="\x1b[1;3R", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_F4,    .text="\x1b[1;3S", .type=TT_XTERM},
  // rxvt
  {.mods=Mod_Alt, .ksym=XK_F1,    .text="\x1b[11;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_F2,    .text="\x1b[12;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_F3,    .text="\x1b[13;3~", .type=TT_RXVT},
  {.mods=Mod_Alt, .ksym=XK_F4,    .text="\x1b[14;3~", .type=TT_RXVT},

  {.mods=Mod_Alt, .ksym=XK_F5,    .text="\x1b[15;3~", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_F6,    .text="\x1b[17;3~", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_F7,    .text="\x1b[18;3~", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_F8,    .text="\x1b[19;3~", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_F9,    .text="\x1b[20;3~", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_F10,   .text="\x1b[21;3~", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_F11,   .text="\x1b[23;3~", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_F12,   .text="\x1b[24;3~", .type=TT_ANY},

  {.mods=Mod_Shift, .ksym=XK_F1,  .text="\x1b[23~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F2,  .text="\x1b[24~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F3,  .text="\x1b[25~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F4,  .text="\x1b[26~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F5,  .text="\x1b[28~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F6,  .text="\x1b[29~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F7,  .text="\x1b[31~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F8,  .text="\x1b[32~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F9,  .text="\x1b[33~", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_F10, .text="\x1b[34~", .type=TT_ANY},

  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F1, .text="\x1b[11@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F2, .text="\x1b[12@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F3, .text="\x1b[13@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F4, .text="\x1b[14@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F5, .text="\x1b[15@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F6, .text="\x1b[17@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F7, .text="\x1b[18@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F8, .text="\x1b[19@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F9, .text="\x1b[20@", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_F10,.text="\x1b[21@", .type=TT_ANY},

  // xterm
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F1, .text="\x1b[1;4P", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F2, .text="\x1b[1;4Q", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F3, .text="\x1b[1;4R", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F4, .text="\x1b[1;4S", .type=TT_XTERM},
  // rxvt
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F1, .text="\x1b[11;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F2, .text="\x1b[12;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F3, .text="\x1b[13;4~", .type=TT_RXVT},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F4, .text="\x1b[14;4~", .type=TT_RXVT},

  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F5, .text="\x1b[15;4~", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F6, .text="\x1b[17;4~", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F7, .text="\x1b[18;4~", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F8, .text="\x1b[19;4~", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F9, .text="\x1b[20;4~", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F10,.text="\x1b[21;4~", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F11,.text="\x1b[23;4~", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_F12,.text="\x1b[24;4~", .type=TT_ANY},

  // xterm
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F1, .text="\x1b[1;7P", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F2, .text="\x1b[1;7Q", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F3, .text="\x1b[1;7R", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F4, .text="\x1b[1;7S", .type=TT_XTERM},
  // rxvt
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F1, .text="\x1b[11;7~", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F2, .text="\x1b[12;7~", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F3, .text="\x1b[13;7~", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F4, .text="\x1b[14;7~", .type=TT_RXVT},

  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F5, .text="\x1b[15;7~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F6, .text="\x1b[17;7~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F7, .text="\x1b[18;7~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F8, .text="\x1b[19;7~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F9, .text="\x1b[20;7~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F10,.text="\x1b[21;7~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F11,.text="\x1b[23;7~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_F12,.text="\x1b[24;7~", .type=TT_ANY},

  // xterm
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F1, .text="\x1b[1;8P", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F2, .text="\x1b[1;8Q", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F3, .text="\x1b[1;8R", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F4, .text="\x1b[1;8S", .type=TT_XTERM},
  // rxvt
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F1, .text="\x1b[11;8~", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F2, .text="\x1b[12;8~", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F3, .text="\x1b[13;8~", .type=TT_RXVT},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F4, .text="\x1b[14;8~", .type=TT_RXVT},

  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F5, .text="\x1b[15;8~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F6, .text="\x1b[17;8~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F7, .text="\x1b[18;8~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F8, .text="\x1b[19;8~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F9, .text="\x1b[20;8~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F10,.text="\x1b[21;8~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F11,.text="\x1b[23;8~", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F12,.text="\x1b[24;8~", .type=TT_ANY},

  // xterm
  {.ksym=XK_Up,    .text="\x1bOA", .type=TT_XTERM},
  {.ksym=XK_Down,  .text="\x1bOB", .type=TT_XTERM},
  {.ksym=XK_Right, .text="\x1bOC", .type=TT_XTERM},
  {.ksym=XK_Left,  .text="\x1bOD", .type=TT_XTERM},

  // rxvt
  {.ksym=XK_Up,    .text="\x1b[A", .type=TT_RXVT},
  {.ksym=XK_Down,  .text="\x1b[B", .type=TT_RXVT},
  {.ksym=XK_Right, .text="\x1b[C", .type=TT_RXVT},
  {.ksym=XK_Left,  .text="\x1b[D", .type=TT_RXVT},
  //
  /*
  {.ksym=XK_KP_Up,    .text="\x1bOA", .type=TT_RXVT},
  {.ksym=XK_KP_Down,  .text="\x1bOB", .type=TT_RXVT},
  {.ksym=XK_KP_Right, .text="\x1bOC", .type=TT_RXVT},
  {.ksym=XK_KP_Left,  .text="\x1bOD", .type=TT_RXVT},
  */
  //

  ////// {.ksym=XK_kpad+Up, .text="\x1bOA", .type=TT_ANY},
  ////// {.ksym=XK_kpad+Down, .text="\x1bOB", .type=TT_ANY},
  ////// {.ksym=XK_kpad+Right, .text="\x1bOC", .type=TT_ANY},
  ////// {.ksym=XK_kpad+Left, .text="\x1bOD", .type=TT_ANY},

  {.mods=Mod_Shift, .ksym=XK_Up,    .text="\x1b[1;2A", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_Down,  .text="\x1b[1;2B", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_Right, .text="\x1b[1;2C", .type=TT_ANY},
  {.mods=Mod_Shift, .ksym=XK_Left,  .text="\x1b[1;2D", .type=TT_ANY},

  {.mods=Mod_Alt, .ksym=XK_Up,    .text="\x1b[1;3A", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_Down,  .text="\x1b[1;3B", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_Right, .text="\x1b[1;3C", .type=TT_ANY},
  {.mods=Mod_Alt, .ksym=XK_Left,  .text="\x1b[1;3D", .type=TT_ANY},

  {.mods=Mod_Shift|Mod_Alt, .ksym=XK_Up,    .text="\x1b[1;4A", .type=TT_ANY},
  {.mods=Mod_Shift|Mod_Alt, .ksym=XK_Down,  .text="\x1b[1;4B", .type=TT_ANY},
  {.mods=Mod_Shift|Mod_Alt, .ksym=XK_Right, .text="\x1b[1;4C", .type=TT_ANY},
  {.mods=Mod_Shift|Mod_Alt, .ksym=XK_Left,  .text="\x1b[1;4D", .type=TT_ANY},

  {.mods=Mod_Ctrl, .ksym=XK_Up,    .text="\x1b[1;5A", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_Down,  .text="\x1b[1;5B", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_Right, .text="\x1b[1;5C", .type=TT_ANY},
  {.mods=Mod_Ctrl, .ksym=XK_Left,  .text="\x1b[1;5D", .type=TT_ANY},

  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Up,    .text="\x1b[1;6A", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Down,  .text="\x1b[1;6B", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Right, .text="\x1b[1;6C", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Left,  .text="\x1b[1;6D", .type=TT_ANY},

  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Up,    .text="\x1b[1;7A", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Down,  .text="\x1b[1;7B", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Right, .text="\x1b[1;7C", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Left,  .text="\x1b[1;7D", .type=TT_ANY},

  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Up,    .text="\x1b[1;8A", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Down,  .text="\x1b[1;8B", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Right, .text="\x1b[1;8C", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Left,  .text="\x1b[1;8D", .type=TT_ANY},

  // xterm does this (my: "\x1b[1;2Z)"
  {.mods=Mod_Shift, .ksym=XK_Tab, .text="\x1b[Z", .type=TT_ANY},
  // x11 is fucked: this is for shift+tab
  {.mods=Mod_Shift, .ksym=XK_ISO_Left_Tab, .text="\x1b[Z", .type=TT_ANY},
  // my own
  //{.mods=Mod_Alt, .ksym=Tab, .text="\x1b[1;3Z", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift,          .ksym=XK_Tab, .text="\x1b[1;4Z", .type=TT_ANY},
  {.mods=Mod_Ctrl,                   .ksym=XK_Tab, .text="\x1b[1;5Z", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift,         .ksym=XK_Tab, .text="\x1b[1;6Z", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt,           .ksym=XK_Tab, .text="\x1b[1;7Z", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Tab, .text="\x1b[1;8Z", .type=TT_ANY},


  // k8 yterm extensions
  // space
  {.mods=Mod_Shift,                  .ksym=XK_space, .text="\x1b[32;2Y", .type=TT_ANY},
  {.mods=Mod_Alt,                    .ksym=XK_space, .text="\x1b\x20", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift,          .ksym=XK_space, .text="\x1b[32;4Y", .type=TT_ANY},
  //{.mods=Mod_Ctrl,                   .ksym=XK_space, .text="\x1b[32;5Y", .type=TT_ANY}, // this is 0x00
  {.mods=Mod_Ctrl|Mod_Shift,         .ksym=XK_space, .text="\x1b[32;6Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt,           .ksym=XK_space, .text="\x1b[32;7Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_space, .text="\x1b[32;8Y", .type=TT_ANY},

  // Return
  {.mods=Mod_Shift,                  .ksym=XK_Return, .text="\x1b[13;2Y", .type=TT_ANY},
  {.mods=Mod_Alt,                    .ksym=XK_Return, .text="\x1b\x0d", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift,          .ksym=XK_Return, .text="\x1b[13;4Y", .type=TT_ANY},
  {.mods=Mod_Ctrl,                   .ksym=XK_Return, .text="\x1b[13;5Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift,         .ksym=XK_Return, .text="\x1b[13;6Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt,           .ksym=XK_Return, .text="\x1b[13;7Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Return, .text="\x1b[13;8Y", .type=TT_ANY},

  {                                  .ksym=XK_F11, .text="\x1b[111Y", .type=TT_ANY},
  {                                  .ksym=XK_F12, .text="\x1b[112Y", .type=TT_ANY},
  {.mods=Mod_Shift,                  .ksym=XK_F11, .text="\x1b[111;2Y", .type=TT_ANY},
  {.mods=Mod_Shift,                  .ksym=XK_F12, .text="\x1b[112;2Y", .type=TT_ANY},
  {.mods=Mod_Alt,                    .ksym=XK_F11, .text="\x1b[111;3Y", .type=TT_ANY},
  {.mods=Mod_Alt,                    .ksym=XK_F12, .text="\x1b[112;3Y", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift,          .ksym=XK_F11, .text="\x1b[111;4Y", .type=TT_ANY},
  {.mods=Mod_Alt|Mod_Shift,          .ksym=XK_F12, .text="\x1b[112;4Y", .type=TT_ANY},
  {.mods=Mod_Ctrl,                   .ksym=XK_F11, .text="\x1b[111;5Y", .type=TT_ANY},
  {.mods=Mod_Ctrl,                   .ksym=XK_F12, .text="\x1b[112;5Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift,         .ksym=XK_F11, .text="\x1b[111;6Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Shift,         .ksym=XK_F12, .text="\x1b[112;6Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt,           .ksym=XK_F11, .text="\x1b[111;7Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt,           .ksym=XK_F12, .text="\x1b[112;7Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F11, .text="\x1b[111;8Y", .type=TT_ANY},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_F12, .text="\x1b[112;8Y", .type=TT_ANY},

  // //////// xterm //////// //
  {.ksym=XK_Home,      .text="\x1b[1~", .type=TT_XTERM},
  {.ksym=XK_End,       .text="\x1b[4~", .type=TT_XTERM},
  {.ksym=XK_Insert,    .text="\x1b[2~", .type=TT_XTERM},
  {.ksym=XK_Delete,    .text="\x1b[3~", .type=TT_XTERM},
  {.ksym=XK_Prior,     .text="\x1b[5~", .type=TT_XTERM},
  {.ksym=XK_Next,      .text="\x1b[6~", .type=TT_XTERM},

  {.mods=Mod_Shift, .ksym=XK_Home,   .text="\x1b[1;2H", .type=TT_XTERM},
  {.mods=Mod_Shift, .ksym=XK_End,    .text="\x1b[1;2F", .type=TT_XTERM},
  {.mods=Mod_Shift, .ksym=XK_Insert, .text="\x1b[2;2~", .type=TT_XTERM},
  {.mods=Mod_Shift, .ksym=XK_Delete, .text="\x1b[3;2~", .type=TT_XTERM},
  {.mods=Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5;2~", .type=TT_XTERM},
  {.mods=Mod_Shift, .ksym=XK_Next,   .text="\x1b[6;2~", .type=TT_XTERM},

  {.mods=Mod_Alt, .ksym=XK_Home,   .text="\x1b[1;3H", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_End,    .text="\x1b[1;3F", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_Insert, .text="\x1b[2;3~", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_Delete, .text="\x1b[3;3~", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_Prior,  .text="\x1b[5;3~", .type=TT_XTERM},
  {.mods=Mod_Alt, .ksym=XK_Next,   .text="\x1b[6;3~", .type=TT_XTERM},

  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Home,   .text="\x1b[1;4H", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_End,    .text="\x1b[1;4F", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Insert, .text="\x1b[2;4~", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Delete, .text="\x1b[3;4~", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5;4~", .type=TT_XTERM},
  {.mods=Mod_Alt|Mod_Shift, .ksym=XK_Next,   .text="\x1b[6;4~", .type=TT_XTERM},

  {.mods=Mod_Ctrl, .ksym=XK_Home,   .text="\x1b[1;5H", .type=TT_XTERM},
  {.mods=Mod_Ctrl, .ksym=XK_End,    .text="\x1b[1;5F", .type=TT_XTERM},
  {.mods=Mod_Ctrl, .ksym=XK_Insert, .text="\x1b[2;5~", .type=TT_XTERM},
  {.mods=Mod_Ctrl, .ksym=XK_Delete, .text="\x1b[3;5~", .type=TT_XTERM},
  {.mods=Mod_Ctrl, .ksym=XK_Prior,  .text="\x1b[5;5~", .type=TT_XTERM},
  {.mods=Mod_Ctrl, .ksym=XK_Next,   .text="\x1b[6;5~", .type=TT_XTERM},

  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Home,   .text="\x1b[1;6H", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_End,    .text="\x1b[1;6F", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Insert, .text="\x1b[2;6~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Delete, .text="\x1b[3;6~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5;6~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Shift, .ksym=XK_Next,   .text="\x1b[6;6~", .type=TT_XTERM},

  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Home,   .text="\x1b[1;7H", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_End,    .text="\x1b[1;7F", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Insert, .text="\x1b[2;7~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Delete, .text="\x1b[3;7~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Prior,  .text="\x1b[5;7~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt, .ksym=XK_Next,   .text="\x1b[6;7~", .type=TT_XTERM},

  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Home,   .text="\x1b[1;8H", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_End,    .text="\x1b[1;8F", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Insert, .text="\x1b[2;8~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Delete, .text="\x1b[3;8~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Prior,  .text="\x1b[5;8~", .type=TT_XTERM},
  {.mods=Mod_Ctrl|Mod_Alt|Mod_Shift, .ksym=XK_Next,   .text="\x1b[6;8~", .type=TT_XTERM},

  {.text=NULL},
};
