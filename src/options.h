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

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "editor.h"

/*
 * ade's settings: the editor's -- tab width, colours, a font -- from aed's
 * library, read from and written to /config/ade.ini, a file of ade's own.
 * The library does the reading, applying and the screen that changes them;
 * this names ade in the file and hands the library ade's schema.
 */

// Reads the settings and applies them as ade starts: its ed_program.settings.
// The first run writes a fresh /config/ade.ini to edit.
const char* ade_settings(editor* ed);

// Options -> Settings, and CTRL+E: shows the settings, applies what is
// changed, and writes it to the file.
void ade_cmd_settings(editor* ed);

#endif  // _OPTIONS_H_
