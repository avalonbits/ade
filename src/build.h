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

#ifndef _BUILD_H_
#define _BUILD_H_

#include "editor.h"
#include "session.h"

/*
 * Building and running, through hub -- see away.h for how ade leaves and
 * comes back.
 *
 * Without a project, CTRL+B builds the file on screen:
 *
 *   .c           acc <file> -o <name>.bin -errors ade.err
 *   .s, .asm     zap <file> <name>.bin -c -e ade.err
 *
 * With a project.ini in the current directory, it builds the project: hub
 * runs `Obey build.obey`, a command per file, the link and the documentation
 * -- see project.h. CTRL+R builds and then runs the program, on hub's screen,
 * waiting for a key after; ade comes back saying what it returned.
 */

// CTRL+B: builds.
void build(editor* ed);

// CTRL+R: builds, then runs what was built.
void build_run(editor* ed);

// ALT+F5: what the last program left on the screen, as hub captured it, until
// a key is pressed.
void build_user_screen(editor* ed);

// How the last build went, for the menu bar: "built", "failed", what the
// program returned -- "demo returned 0" -- or "" before any.
const char* build_status(void);

// Coming back from a build (see away_come_back): where ade lands -- moved to
// the first error, after a failed build -- and the menu bar's status.
void build_landing(editor* ed, const session* s, int* window, int* line, int* x);

// ...and, once the screen is shown, what a failed build said: its error, the
// list of them, or how it failed.
void build_report(editor* ed, const session* s);

#endif  // _BUILD_H_
