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

#ifndef _FILES_H_
#define _FILES_H_

#include <stdbool.h>

#include "editor.h"

/*
 * The file viewer CTRL+O and CTRL+SHIFT+O open: the directory ade started in
 * -- the project's, when there is one -- listed on the whole screen,
 * directories first, then files, each by name. UP and DOWN choose, and
 * RETURN opens a file or goes into a directory. Typing narrows the list to
 * the names holding what is typed; RETURN with nothing left opens the typed
 * name as a new file. F7, or the [ New directory ] row, makes a directory.
 *
 * It keeps to the directory ade started in: there is no ".." above it, and a
 * typed name that resolves outside it is refused.
 *
 * True with the chosen name in `out` -- relative to the current directory,
 * or as typed when typed -- and false when the user left with ESC. Either
 * way the screen is the viewer's: the caller repaints.
 */
bool files_pick(editor* ed, const char* title, char* out, int max);

#endif  // _FILES_H_
