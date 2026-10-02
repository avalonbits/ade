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

#include "text.h"

void text_start(text* t, char* buf, int max) {
    t->buf = buf;
    t->n = 0;
    t->max = max;
    t->cut = false;
    if (max > 0) {
        buf[0] = 0;
    }
}

// One byte, if there is room for it and the NUL after it.
static void put(text* t, char c) {
    if (t->n + 1 < t->max) {
        t->buf[t->n++] = c;
        t->buf[t->n] = 0;
    } else {
        t->cut = true;
    }
}

void text_str(text* t, const char* s) {
    while (*s != 0) {
        put(t, *s++);
    }
}

void text_strn(text* t, const char* s, int most) {
    for (int i = 0; i < most && s[i] != 0; i++) {
        put(t, s[i]);
    }
}

void text_int(text* t, long v) {
    // In unsigned, so the most negative long converts too.
    unsigned long u = v < 0 ? 0UL - (unsigned long) v : (unsigned long) v;
    char digits[12];
    int i = 0;
    do {
        digits[i++] = (char) ('0' + u % 10);
        u /= 10;
    } while (u != 0);
    if (v < 0) {
        put(t, '-');
    }
    while (i > 0) {
        put(t, digits[--i]);
    }
}

void text_hex(text* t, unsigned long v, int digits) {
    for (int i = digits - 1; i >= 0; i--) {
        put(t, "0123456789abcdef"[(v >> (4 * i)) & 0xF]);
    }
}

int text_len(const text* t) {
    return t->n;
}
