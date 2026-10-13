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

#ifndef _LOOKUP_H_
#define _LOOKUP_H_

#include "editor.h"
#include "session.h"

/*
 * Looking things up in ref, through hub -- see away.h for how ade leaves and
 * comes back.
 *
 * ref reads documents from /config/ref/docs/<name>.dat: "acc" is acc's C
 * library, "mos" the Agon's own documentation. A project's documentation is
 * ref-doc/<doc>.dat in its directory -- see project.h.
 */

// CTRL+F11: looks up the word at the cursor -- or the selection, on one line
// -- in the project's document first, if it has one, then in acc's for a .c
// or .h file and in mos's for anything else.
void lookup_word(editor* ed);

// CTRL+ALT+F11: ref at mos's contents, its start page.
void lookup_contents(editor* ed);

// Coming back from ref (see away_come_back): what to say, if anything. ref
// exits 0 having shown something and 100 having found nothing.
void lookup_report(editor* ed, const session* s);

#endif  // _LOOKUP_H_
