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

#include "shell.h"

#include <hub/hub.h>

#include "away.h"
#include "say.h"

// The shell job's command, which hub 0.6 does not run: HUB_SHELL says what
// to do. An older hub does not know HUB_SHELL and runs the command instead.
// Named so that no program answers to it, it fails there -- where "shell"
// might run a shell.bin that returns 0, which ade would take for the shell.
#define SHELL_JOB "ade-shell-needs-hub-0.6"

void shell_open(editor* ed) {
    if (!away_save_windows(ed)) {
        return;
    }
    session* s = away_keep(ed, SESSION_SHELL);
    if (s != NULL) {
        away_hand_over(ed, s, SHELL_JOB, HUB_SHELL | HUB_USER_PROGRAM, NULL, 0,
                       "open its shell");
    }
}

void shell_report(editor* ed) {
    // exit ends the shell with 0; anything else is an older hub.
    if (hub_last_result() != 0) {
        say(ed, "The shell needs hub 0.6 or later");
    }
}
