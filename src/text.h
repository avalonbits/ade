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

#ifndef _TEXT_H_
#define _TEXT_H_

#include <stdbool.h>

/*
 * A string built in a buffer a piece at a time, where ade used snprintf:
 * that call linked agondev's formatter, about 5 KB, for the few kinds of
 * piece ade puts in a message or a command -- strings, numbers, hex. What
 * does not fit is cut at the buffer's end, always NUL-terminated, and `cut`
 * says it happened.
 */
typedef struct {
    char* buf;
    int n;
    int max;
    bool cut;
} text;

// Starts `t` on buf[0, max), empty.
void text_start(text* t, char* buf, int max);
// Appends `s`; with text_strn, at most `most` bytes of it.
void text_str(text* t, const char* s);
void text_strn(text* t, const char* s, int most);
// Appends `v` in decimal, a '-' first if it is negative.
void text_int(text* t, long v);
// Appends `v` in hex, lowercase, `digits` of them, zeros in front.
void text_hex(text* t, unsigned long v, int digits);
// The length built, and whether all of it fitted.
int text_len(const text* t);

#endif  // _TEXT_H_
