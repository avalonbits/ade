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

#include "errors.h"

#include <agon/mos.h>
#include <string.h>

#include "cmd_ops.h"
#include "say.h"
#include "text_buffer.h"
#include "user_input.h"
#include "windows.h"

#define ERR_MAX 16

// ERR_FILE as read, each line made a string, and where each starts.
static char errs[1024];
static int err_at[ERR_MAX];
static int nerrs;
static int chosen;                      // the one last gone to, for the list

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// The first ':' followed by a digit: where the place starts. Looked for
// rather than the first ':', since a file's name may hold a drive's colon.
static const char* place_of(const char* e) {
    while (*e != 0 && !(e[0] == ':' && is_digit(e[1]))) {
        e++;
    }

    return *e != 0 ? e : NULL;
}

// The file, line and column error `e` points at. False when it has no line.
static bool parse_place(const char* e, char* file, int fmax, int* line, int* col) {
    const char* colon = place_of(e);
    if (colon == NULL || colon == e || colon - e >= fmax) {
        return false;
    }
    memcpy(file, e, (size_t) (colon - e));
    file[colon - e] = 0;
    *line = 0;
    *col = 0;
    const char* q = colon + 1;
    while (is_digit(*q)) {
        *line = *line * 10 + (*q++ - '0');
    }
    if (*q == ':') {
        q++;
        while (is_digit(*q)) {
            *col = *col * 10 + (*q++ - '0');
        }
    }

    return *line > 0;
}

void errors_clear(void) {
    mos_del(ERR_FILE);
}

void errors_read(void) {
    nerrs = 0;
    chosen = 0;
    const char fh = mos_fopen(ERR_FILE, FA_READ);
    if (fh == 0) {
        return;
    }
    const int n = (int) mos_fread(fh, errs, sizeof(errs) - 1);
    mos_fclose(fh);
    if (n <= 0) {
        return;
    }
    errs[n] = 0;

    // Every line is an error, whether it says where or not.
    int start = 0;
    while (start < n && nerrs < ERR_MAX) {
        int end = start;
        while (end < n && errs[end] != '\r' && errs[end] != '\n') {
            end++;
        }
        errs[end] = 0;
        if (errs[start] != 0) {
            err_at[nerrs++] = start;
        }
        start = end + 1;
        while (start < n && (errs[start] == '\r' || errs[start] == '\n')) {
            start++;
        }
    }
}

int errors_count(void) {
    return nerrs;
}

int errors_window(editor* ed, int k, int* line, int* x) {
    static char file[TB_FNAME_MAX];
    int col = 0;
    if (!parse_place(errs + err_at[k], file, TB_FNAME_MAX, line, &col)) {
        return -1;
    }
    *x = col > 0 ? col - 1 : 0;         // the tools count columns from 1
    const int w = win_find(ed, file);

    return w >= 0 ? w : win_open_named(ed, file);
}

const char* errors_text(int k) {
    const char* e = errs + err_at[k];
    const char* q = place_of(e);
    if (q == NULL) {
        return e;                       // no place: all of it
    }

    // Past the line, and the column if there is one.
    const char* place = q + 1;
    bool line_zero = true;
    for (int field = 0; field < 2 && q[0] == ':' && is_digit(q[1]); field++) {
        q++;
        while (is_digit(*q)) {
            line_zero = line_zero && (field > 0 || *q == '0');
            q++;
        }
    }
    if (*q != ':') {
        return e;                       // not that shape: all of it
    }
    if (!line_zero) {
        return place;                   // "3:12: error: ..."
    }

    // Line 0 is in no file: just what it says.
    q++;
    while (*q == ' ') {
        q++;
    }

    return *q != 0 ? q : e;
}

// Goes to error k and shows what it says.
static void go_to(editor* ed, int k) {
    int line = 0;
    int x = 0;
    const int w = errors_window(ed, k, &line, &x);
    if (w >= 0) {
        win_show_at(ed, w, line, x);
    }
    chosen = k;
    say(ed, errors_text(k));
}

void errors_list(editor* ed) {
    if (nerrs == 0) {
        say(ed, "The last build left no errors");
        return;
    }
    screen* scr = &ed->scr_;
    user_input* ui = &ed->ui_;
    const char top = scr->v_->topY_;
    const char bottom = ui->ypos_;
    const int width = scr->v_->cols_ < 255 ? scr->v_->cols_ : 255;
    static char row[256];

    int at = chosen;
    for (;;) {
        char y = ui_modal_title(scr, top, width, "  ERRORS");
        for (int k = 0; k < nerrs && y < bottom; k++) {
            int c = ui_put_at(row, 0, width, k == at ? "  > " : "    ");
            c = ui_put_at(row, c, width, errs + err_at[k]);
            scr_write_line(scr, y++, row, c);
        }
        ui_modal_fill(scr, y, bottom,
                      "  UP/DOWN to choose, RETURN to go there, ESC to stay", 52);

        const key_press kp = ks_wait(ui->keys_);
        const pick_key act = ui_modal_move(kp.vkey, &at, nerrs - 1);
        if (act == PICK_LEAVE) {
            cmd_restore_after_modal(ed, false);
            return;
        }
        if (act == PICK_TAKE) {
            go_to(ed, at);
            return;
        }
    }
}
