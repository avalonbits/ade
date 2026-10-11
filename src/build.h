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
 * Run queues the program after the build, on hub's own screen and with a key
 * to press when it ends; ade comes back saying what it returned.
 *
 * With a project.ini in the current directory, F9 builds the project instead:
 * a command per file and the link, in build.obey, run by MOS's Obey as one
 * of hub's jobs and stopping at the first that fails -- see project.h.
 */

// The command F9 is bound to, and Build on the menu.
void build(editor* ed);

// Builds, then runs what was built -- the project's program, or the file's
// .bin -- and comes back. CTRL+F9, and Run on the menu.
void build_run(editor* ed);

// Drops the user at hub's prompt, every window saved first, and brings ade
// back as it was when they type exit. Needs hub 0.6 (HUB_SHELL); an older hub
// fails the job, and ade comes back saying so.
void build_shell(editor* ed);

// The errors the last build left, as a list to go to one from; shown on
// coming back from a build that left more than one. ALT+F9, and on the menu.
void build_errors(editor* ed);

// Whether ade was started by hub to come back from a build, and puts the
// session back if so: call after ed_init_for, with the windows started. The
// first file to open is `first_file` -- see build_first_file.
void build_resume(editor* ed);

// Build -> New project: asks for the program's name and writes a project.ini
// for it, with no list of files -- every .c, .s and .asm in the directory is
// the project -- unless there is one already, which is left as it is.
void build_new_project(editor* ed);

// The name project.ini gives the program, or NULL without a project: read
// the first time it is asked for, and again at each build.
const char* build_project(void);

// The project's first file -- the first of its files line, or else the first
// by name of those in the directory -- for ade started with no file to open;
// NULL with no project, or one that cannot be read.
const char* build_project_file(void);

// The user screen, ALT+F5: what the last program Run ran left on the screen,
// as hub captured it, until a key is pressed; then ade again.
void build_user_screen(editor* ed);

// Back to the screen mode ade left in, before the editor starts on resume: a
// program run in between may have changed it, and hub does not put it back.
void build_resume_screen(void);

// How the last build went, for the menu bar: "built", "failed", or what the
// program run after it returned -- "demo returned 0" -- and "" before any.
const char* build_status(void);

// The last thing a build said on the message row, as it was shown -- for the
// scripted test, which cannot read that row back off the screen.
const char* build_said(void);

// Whether each mode change ade made coming back -- from a build, or from
// ALT+F5 -- had finished before it went on: 'y' or 'n' each, oldest first.
// For the scripted test, which cannot slow the VDP down to see it.
const char* build_modes_settled(void);

// The file window 1 held when the build began, for ed_init_for to open on
// resume; NULL when there is no session to go back to.
const char* build_first_file(void);

#endif  // _BUILD_H_
