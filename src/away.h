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

#ifndef _AWAY_H_
#define _AWAY_H_

#include <stdbool.h>

#include "editor.h"
#include "session.h"

/*
 * Leaving ade for hub, and coming back.
 *
 * ade, acc, zap and ref all load at the same address, so ade cannot stay in
 * memory while another program runs. To build, look something up or open
 * the shell, ade:
 *
 *   1. saves every window that has changes (away_save_windows);
 *   2. writes down its session -- which files are open in which windows,
 *      where each cursor is, the screen mode -- in a block of hub's memory
 *      (away_keep);
 *   3. asks hub to run the job and then `ade -resume`, and quits
 *      (away_hand_over).
 *
 * hub runs the job, then starts ade again with -resume. ade puts the screen
 * mode back (away_screen_back), opens window 1's file (away_first_file), and
 * then the rest of the session, and says how the job went (away_come_back).
 */

// Saves every window with changes. False, having said why, when one has no
// name or cannot be saved.
bool away_save_windows(editor* ed);

// Writes the session for a job of `kind` (SESSION_BUILD, _SHELL or _REF) into
// hub's block, and returns it for the caller to add to. NULL, having said so,
// when hub has no room.
session* away_keep(editor* ed, unsigned char kind);

// Queues `job` with hub's `flags`, then `then` (when not NULL) with
// `then_flags`, then `ade -resume`, and has ade quit. False, having said that
// hub would not `what`, when hub refuses any of it.
bool away_hand_over(editor* ed, session* s, const char* job, unsigned char flags,
                    const char* then, unsigned char then_flags, const char* what);

// On -resume, before the editor starts: back to the screen mode ade left in,
// which the job may have changed.
void away_screen_back(void);

// On -resume: the file window 1 held, for the editor to start on; NULL when
// there is no session to come back to.
const char* away_first_file(void);

// On -resume, once the editor has started: every window back where it was,
// the one that was on screen shown -- or, after a failed build, the file and
// line of its first error -- and what the job said.
void away_come_back(editor* ed);

#endif  // _AWAY_H_
