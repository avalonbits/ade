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
 * ade's windows: each a file, shown full-screen in a view of its own, one at a
 * time. Up to ADE_WINDOWS of them, of which ADE_LOADED at most have their file
 * loaded: each loaded one gets ADE_DOC_KB of text and an undo log, and a paged
 * one holds two of the seven files MOS lets a program have open. Showing a
 * window that is not loaded puts away the loaded one used longest ago whose
 * work is saved -- keeping its name and cursor, losing its undo -- or, when
 * none is, asks to save one first.
 *
 * Window 1 is the file ade started with. Any window can be closed but the
 * last, and the ones after it move down a number.
 */
#define ADE_WINDOWS 10
#define ADE_LOADED  3
#define ADE_NAME_MAX 128
#define ADE_DOC_KB  48

// Takes over window 1 from an editor ed_init_for has just started.
void win_start(editor* ed);

// Closes every window but the editor's own document, which ed_destroy takes
// with the editor -- opened again, empty, if its window was closed.
void win_finish(editor* ed);

// The commands ade binds: open a file in a new window, go to the next one,
// close the one on screen, and choose from a list of them. And leaving, which
// asks about every window with unsaved changes rather than only the one shown.
void win_open(editor* ed);
void win_next(editor* ed);
void win_prev(editor* ed);
// To window 1 to 10, and nothing when there is no such window.
void win_to_1(editor* ed);
void win_to_2(editor* ed);
void win_to_3(editor* ed);
void win_to_4(editor* ed);
void win_to_5(editor* ed);
void win_to_6(editor* ed);
void win_to_7(editor* ed);
void win_to_8(editor* ed);
void win_to_9(editor* ed);
void win_to_10(editor* ed);
void win_close(editor* ed);
void win_list(editor* ed);
void win_quit(editor* ed);

// Opens `fname` in a new window without asking for a name, and returns the
// window it is in (0-based), or -1 having said why it could not. Which window
// holds a file already, or -1; and showing window `i`.
int win_open_named(editor* ed, const char* fname);
int win_find(editor* ed, const char* fname);
void win_go(editor* ed, int i);

// A window that was open when ade left for a build, back as it was but put
// away: loaded when it is shown. Returns its number, or -1.
int win_restore(editor* ed, const char* fname, int line, int x);

// What is open, for anything that reports on it: the document in window `i`
// (0-based) when it is loaded, else NULL; its file's name, loaded or not, or
// NULL for no such window; whether it is loaded; where its cursor is, x from
// 0; and how many windows there are.
document* win_doc(editor* ed, int i);
const char* win_name(editor* ed, int i);
bool win_loaded(int i);
void win_where(editor* ed, int i, int* line, int* x);
int win_count(void);

// Puts window i's cursor at (line, x), x from 0, and its view round it,
// drawing nothing: the window on screen shows the old place until it is
// repainted, and any other is painted there when it is shown, loaded or not.
void win_place(editor* ed, int i, int line, int x);
int win_current(void);

// The open windows' numbers, the one on screen in brackets -- "1 [2] 3" --
// into out; returns its length.
int win_strip(char* out, int max);

#endif  // _WINDOWS_H_
