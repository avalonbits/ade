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

#ifndef _SAY_H_
#define _SAY_H_

#include "editor.h"

// Shows `msg` on the bottom row and waits for a key. A message too long for
// the row is cut to fit, ending "...": written past the row's end it would
// wrap, and the VDP would scroll the whole screen up a line.
void say(editor* ed, const char* msg);

// The last message shown, as it was shown -- for the scripted test, which
// cannot read the bottom row back off the screen.
const char* said_last(void);

#endif  // _SAY_H_
