/*
 * Copyright (C) 2026  Igor Cananea <icc@avalonbits.com>
 * Author: Igor Cananea <icc@avalonbits.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MENU_H_
#define _MENU_H_

#include "editor.h"

/*
 * ade's menu bar, on the row under the title: File, Edit, Search, Build, Run,
 * Options.
 *
 * F10 opens it, as in Turbo Pascal. LEFT and RIGHT move between menus, UP and
 * DOWN through the one pulled down, RETURN chooses and ESC -- or F10 again --
 * leaves. Everything on a menu is also on a key, shown beside it; choosing it
 * runs exactly what the key would, through ed_handle.
 */

// Puts the bar on the row under the title, which takes that row from the
// text. Before the editor first paints -- from ed_program.settings, which runs
// as soon as the screen exists -- so the text is drawn where it belongs the
// first time rather than painted twice, and before any window makes its view.
void menu_attach(editor* ed);

// Draws the bar again once the windows are started, so it shows them.
void menu_start(editor* ed);

// The bar's text into bar, at most max: the menus' names, the open
// one in brackets, and at the right the project's program and the open
// windows, the one on screen in brackets. Returns its length.
int menu_bar_text(screen* scr, char* bar, int max);

// The command F10 is bound to.
void menu_open(editor* ed);

// Where the menu pulled down last was drawn: its columns, from textX_ for
// cols_, and its rows, from topY_ -- the border -- to bottomY_.
const view* menu_box(void);

#endif  // _MENU_H_
