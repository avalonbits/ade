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

#ifndef _WINDOWS_H_
#define _WINDOWS_H_

#include <stdbool.h>

#include "editor.h"

/*
 * ade's windows: the documents it has open, each shown full-screen in a view
 * of its own, one at a time.
 *
 * Window 1 is the editor's own document, the one ade started with, and stays
 * open. Up to two more open beside it. Three is what memory and file handles
 * allow: each document gets ADE_DOC_KB of text and an undo log, and a paged
 * one holds two of the seven files MOS lets a program have open.
 */
#define ADE_WINDOWS 3
#define ADE_DOC_KB  48

// Takes over window 1 from an editor ed_init_for has just started.
void win_start(editor* ed);

// Closes every window but the first, which ed_destroy takes with the editor.
void win_finish(editor* ed);

// The commands ade binds: open a file in a new window, go to the next one,
// close the one on screen, and choose from a list of them. And leaving, which
// asks about every window with unsaved changes rather than only the one shown.
void win_open(editor* ed);
void win_next(editor* ed);
void win_close(editor* ed);
void win_list(editor* ed);
void win_quit(editor* ed);

// Opens `fname` in a new window without asking for a name, and returns the
// window it is in (0-based), or -1 having said why it could not. Which window
// holds a file already, or -1; and showing window `i`.
int win_open_named(editor* ed, const char* fname);
int win_find(editor* ed, const char* fname);
void win_go(editor* ed, int i);

// What is open, for anything that reports on it: the document in window `i`
// (0-based), or NULL for a free one, and which window is on screen.
document* win_doc(editor* ed, int i);

// Puts window i's cursor at (line, x), x from 0, and its view round it,
// drawing nothing: the window on screen shows the old place until it is
// repainted, and any other is painted there when it is shown.
void win_place(editor* ed, int i, int line, int x);
int win_current(void);

// The open windows' numbers, the one on screen in brackets -- "1 [2] 3" --
// into out; returns its length.
int win_strip(char* out, int max);

#endif  // _WINDOWS_H_
