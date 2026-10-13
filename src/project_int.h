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

#ifndef _PROJECT_INT_H_
#define _PROJECT_INT_H_

// Shared by project.c, scan.c and obey.c, and by nothing else: what the rest
// of ade sees of a project is project.h.

#include <stdbool.h>

#include "project.h"

// ade's default rules: what a project gets for what its project.ini does not
// say. % names are put in by obey.c -- see project.h.
// Objects go in build/, the program in bin/, both under the project's
// directory, so the source is all that is left beside project.ini.
#define RULE_C_TEXT    "acc -c %f -o " PROJ_OBJ_DIR "/%b.o -errors %e"
#define RULE_S_TEXT    "zap %f " PROJ_OBJ_DIR "/%b.o -f acc -c -e %e"
// The link names its objects through a file, %l, since MOS cuts a command
// at 255 characters: a link line naming them was full at fifteen or so.
#define RULE_LINK_TEXT "acc @%l -o " PROJ_BIN_DIR "/%n.bin -errors %e"
#define RULE_RUN_TEXT  PROJ_BIN_DIR "/%n"
// The documentation, when there is any: md2ref makes ref-doc/<doc>.dat of
// the pages build/<doc>.mdl lists, its errors where the tools' go.
#define RULE_DOC_TEXT  "md2ref -e %e %d @" PROJ_OBJ_DIR "/%d" PROJ_DOCS_EXT

// Into msg, what went wrong: up to three pieces, end to end, NULL for none.
void proj_fail(char* msg, int max, const char* a, const char* b, const char* c);

// FNV-1a, 32 bits, over s[0, n), carried on from `h`.
unsigned long proj_hash(unsigned long h, const char* s, int n);

// The rule that builds `file`, or NULL when the project has none for it.
const char* proj_rule_for(const project* p, const char* file);

// Whether `name` is one of the project's excluded files.
bool proj_excluded(const project* p, const char* name);

// Looks through the project's directory, and those under it, for its pages
// of documentation -- and, with `sources`, its files to build (scan.c).
bool proj_scan(project* p, bool sources, char* msg, int max);

// The page after `prev` in the document's order -- the first for NULL --
// or NULL after the last (scan.c).
const char* proj_next_doc(const project* p, const char* prev);

#endif  // _PROJECT_INT_H_
