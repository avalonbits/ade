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

#include "banner.h"

#include <string.h>

#include "version.h"

static const char* BANNER[] = {
    "ADE: Another Development Environment",
    "Version " ADE_VERSION,
    "",
    "Press F10 to access the menus",
};

// Laid out as aed's banner is: framed with + - | rather than box-drawing
// characters, which land on different glyphs from font to font, and centred
// in the text area both ways -- or not shown at all where it does not fit.
static void banner_on(screen* scr) {
    const int n = (int) (sizeof(BANNER) / sizeof(BANNER[0]));
    const int pad = 3;
    int inner = 0;
    for (int i = 0; i < n; i++) {
        const int len = (int) strlen(BANNER[i]);
        if (len > inner) {
            inner = len;
        }
    }
    inner += pad * 2;

    const int boxw = inner + 2;                 // and the two sides
    const int boxh = n + 4;                     // two rules, a blank inside each
    const int width = scr->v_->cols_ < 255 ? scr->v_->cols_ : 255;
    const int rows = scr->v_->bottomY_ - scr->v_->topY_;
    if (boxw > width || boxh > rows) {
        return;
    }
    const int left = (width - boxw) / 2;
    const char first = (char) (scr->v_->topY_ + (rows - boxh) / 2);

    static char line[256];
    for (int r = 0; r < boxh; r++) {
        const bool rule = r == 0 || r == boxh - 1;
        const int body = r - 2;
        const char* mid = !rule && body >= 0 && body < n ? BANNER[body] : NULL;

        // From column zero: the painter pads to the full width, and a line
        // starting further in would leave what was underneath it.
        int at = 0;
        while (at < left) {
            line[at++] = ' ';
        }
        line[at++] = rule ? '+' : '|';
        if (rule) {
            for (int i = 0; i < inner; i++) {
                line[at++] = '-';
            }
        } else {
            const int len = mid != NULL ? (int) strlen(mid) : 0;
            const int lpad = (inner - len) / 2;
            for (int i = 0; i < lpad; i++) {
                line[at++] = ' ';
            }
            for (int i = 0; i < len; i++) {
                line[at++] = mid[i];
            }
            for (int i = lpad + len; i < inner; i++) {
                line[at++] = ' ';
            }
        }
        line[at++] = rule ? '+' : '|';

        scr_write_line(scr, (char) (first + r), line, at);
    }
}

void ade_banner(user_input* ui, screen* scr) {
    (void) ui;

    // On the whole text area, whichever view is current.
    view* was = scr->v_;
    scr_set_view(scr, NULL);
    banner_on(scr);
    scr_set_view(scr, was);
}
