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

#ifndef _SHELL_H_
#define _SHELL_H_

#include "editor.h"

// CTRL+F12: hub's prompt, with every window saved first, until the user types
// exit -- and then ade back as it was. Needs hub 0.6 or later.
void shell_open(editor* ed);

// Coming back from the shell (see away_come_back): says so if this hub has
// no shell.
void shell_report(editor* ed);

#endif  // _SHELL_H_
