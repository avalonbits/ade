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

#include "say.h"

#include <string.h>

#include "user_input.h"

// What ui_message adds after a message, which has to fit on the row with it.
#define DISMISS_LEN 16                  // " (press any key)"

static char said[128];

void say(editor* ed, const char* msg) {
    int room = ed->scr_.barW_ - DISMISS_LEN - 1;
    if (room > (int) sizeof(said) - 1) {
        room = (int) sizeof(said) - 1;
    }
    int n = (int) strlen(msg);
    if (n > room) {
        n = room > 3 ? room - 3 : 0;
        memcpy(said, msg, (size_t) n);
        memcpy(said + n, "...", 3);
        n += 3;
    } else {
        memcpy(said, msg, (size_t) n);
    }
    said[n] = 0;
    ui_message(&ed->ui_, &ed->scr_, said);
}

const char* said_last(void) {
    return said;
}
