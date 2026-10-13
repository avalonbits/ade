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

#ifndef _NEWPROJECT_H_
#define _NEWPROJECT_H_

#include "editor.h"

// CTRL+SHIFT+N, and Build -> New project. With no project.ini here: asks for
// the program's name -- offering the file on screen's -- and writes one, with
// no list of files, so every .c, .s and .asm here and below is the project.
// With one here: asks for a new name, changes only its name line, and deletes
// the old name's bin/<old>.bin and build/<old>.lnk.
void new_project(editor* ed);

#endif  // _NEWPROJECT_H_
