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

#include "mode.h"

#include <agon/mos.h>
#include <stdbool.h>

#define SYSVAR_SCRMODE 0x27

// About three seconds of frames: reached only by a VDP that never answers.
#define MODE_FRAMES 180

static char settled[8];
static int nsettled;

int mode_now(void) {
    return ((volatile unsigned char*) mos_sysvars())[SYSVAR_SCRMODE];
}

void mode_set(int mode) {
    // VDU 22,mode: the change. VDU 23,0,&86: "what mode are you in?"
    const char vdu[] = { 22, (char) mode, 23, 0, (char) 0x86 };
    volatile uint8_t* sysvar = mos_sysvars();
    sysvar[sysvar_vdp_pflags] = 0;
    mos_puts((char*) vdu, sizeof(vdu), 0);

    bool done = false;
    for (int i = 0; i < MODE_FRAMES && !done; i++) {
        done = (sysvar[sysvar_vdp_pflags] & vdp_pflag_mode) != 0;
        if (!done) {
            waitvblank();
        }
    }
    if (nsettled < (int) sizeof(settled) - 1) {
        settled[nsettled++] = done && mode_now() == mode ? 'y' : 'n';
    }
}

const char* mode_settled(void) {
    return settled;
}
