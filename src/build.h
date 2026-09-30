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

#include <stdbool.h>

#include "editor.h"

/*
 * Building, through hub.
 *
 * ade and the tools all load at the same address, so ade cannot stay in
 * memory while one runs. F9 saves what needs saving, writes down the session
 * -- which files are open, where each cursor is, which window is on screen --
 * in a hub block, asks hub to run the tool and then `ade -resume`, and leaves.
 * hub runs the tool, then ade again, which puts the session back and, if the
 * tool failed, reads the error it wrote and goes to it.
 *
 *   .c           acc <file> -o <file>.bin -errors /ade.err
 *   .s, .asm     zap <file> <file>.bin -c -e /ade.err
 *
 * Both write an error as `file:line:column: error: text` and fail with 100.
 *
 * With a project.ini in the current directory, F9 builds the project instead:
 * a job per file and the link, stopping at the first that fails -- see
 * project.h.
 */

// The command F9 is bound to, and Build on the menu.
void build(editor* ed);

// Whether ade was started by hub to come back from a build, and puts the
// session back if so: call after ed_init_for, with the windows started. The
// first file to open is `first_file` -- see build_first_file.
void build_resume(editor* ed);

// The file window 1 held when the build began, for ed_init_for to open on
// resume; NULL when there is no session to go back to.
const char* build_first_file(void);

#endif  // _BUILD_H_
