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

#ifndef _ERRORS_H_
#define _ERRORS_H_

#include "editor.h"

/*
 * The errors a build leaves.
 *
 * acc, zap and md2ref each write an error as `file:line:column: error: text`
 * (md2ref with no column) to ERR_FILE, and fail with 100. A link's error has
 * no place in a file: acc writes it as `acc:0:0: error: text`.
 */

// Where the tools write their errors: in the directory ade started in -- the
// project's -- beside its work.
#define ERR_FILE "ade.err"

// Deletes ERR_FILE, so an earlier build's errors are not taken for the next.
void errors_clear(void);

// Reads ERR_FILE after a failed build: one error a line, up to sixteen.
void errors_read(void);

// How many errors errors_read found.
int errors_count(void);

// Where error k is: its window -- opened if its file has none -- with the
// cursor's line and x there. -1 when it has no place in a file, or its file
// cannot be opened.
int errors_window(editor* ed, int k, int* line, int* x);

// What error k says, for the bottom row: from its line on -- `3:12: error:
// ...` -- since the footer names the file; one with no place, all of it.
const char* errors_text(int k);

// ALT+F9: the list of errors, to choose one and go to it.
void errors_list(editor* ed);

#endif  // _ERRORS_H_
