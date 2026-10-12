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

#ifndef _SESSION_H_
#define _SESSION_H_

#include <stdbool.h>

#include "project.h"
#include "windows.h"

// hub's name for the block the session is kept in.
#define SESSION_TAG "ADE "

// What ade left to do: a build, hub's shell, or a look in ref.
#define SESSION_BUILD 0
#define SESSION_SHELL 1
#define SESSION_REF   2

// The longest word ref is asked for: what hub holds of a command, 93, less
// "ref mos -k ".
#define SESSION_WORD_MAX 82

// What survives the build, in hub's block "ADE ": which files were open, in
// which windows, where each cursor was, and which window was on screen.
typedef struct {
    char magic[4];                      // MAGIC once written
    unsigned short size;                // sizeof(session) when written
    bool ran;                           // the program was queued after the build
    unsigned char kind;                 // SESSION_BUILD, _SHELL or _REF
    char word[SESSION_WORD_MAX + 1];    // what ref was asked to look up
    char program[PROJ_NAME_MAX];        // ...and what it is called
    unsigned char mode;                 // the screen mode ade was in
    unsigned char open[ADE_WINDOWS];
    unsigned char current;
    int line[ADE_WINDOWS];
    int x[ADE_WINDOWS];
    char name[ADE_WINDOWS][TB_FNAME_MAX];  // in hub's memory, not ade's
} session;

// Written into the session's first bytes. A session is ade's only when both
// match: hub keeps a block between ades, and grows it -- keeping its bytes --
// for one that asks for more, so another ade's session can be found in it.
// The size catches a change of layout that MAGIC was not changed for.
#define MAGIC "ADE3"

#endif  // _SESSION_H_
