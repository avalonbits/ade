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

#ifndef _MODE_H_
#define _MODE_H_

// The screen mode now, from MOS's system variables.
int mode_now(void);

// Puts the screen in `mode` and returns once the change has finished.
//
// MOS learns the new screen's size only when the VDP's answer arrives, and on
// real hardware that is well after the VDU 22 that asked for it. Laid out
// sooner, ade would use the old mode's size and show a blank text area. So
// mode_set asks the VDP for its mode after the change -- the VDP takes
// commands in order, so the answer comes once the change is done -- and waits
// for it, up to about three seconds.
void mode_set(int mode);

// How each mode_set went, oldest first: 'y' for finished, 'n' for not. For
// the scripted test, which cannot slow the VDP down to see the difference.
const char* mode_settled(void);

#endif  // _MODE_H_
