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

/*
 * ade, the Agon Development Environment: aed's editor, built from aed's core
 * and UI libraries, as the start of an IDE.
 *
 * This is the first step -- ade opens the file named on the command line and
 * edits it exactly as aed does. Its own keys are its files and leaving; every
 * other key is the editor's own, from ED_KEYS behind them.
 */

#include <stddef.h>

#include "app.h"
#include "editor.h"
#include "text_buffer.h"

// Where ade keeps its files. Grammars, themes and fonts are aed's, shared
// rather than copied: one set on the card serves both programs.
static const app_context ADE_APP = {
    .name       = "ade",
    .cfg_path   = "/config/ade.ini",
    .cfg_old    = NULL,
    .syntax_dir = "/config/aed/syntax",
    .theme_dir  = "/config/aed/themes",
    .font_dir   = "/config/aed",
};

#define C MOD_CTRL

// Both cases of a letter: MOS reports the shifted one when SHIFT is held.
#define LETTER(lo, up, mods, cmd) \
    { lo, mods, 0, cmd }, { up, mods, 0, cmd }

// ade's own keys, the same ones aed uses for the same things.
static const key_binding ADE_BINDINGS[] = {
    LETTER(VK_o, VK_O, C, cmd_open),
    LETTER(VK_s, VK_S, C | MOD_ALT, cmd_save_as),
    LETTER(VK_s, VK_S, C, ed_cmd_save),
    LETTER(VK_q, VK_Q, C, ed_cmd_quit),
};

#undef LETTER
#undef C

static const keymap ADE_KEYS = {
    ADE_BINDINGS, (int) (sizeof(ADE_BINDINGS) / sizeof(ADE_BINDINGS[0])),
    &ED_KEYS,
};

// No settings and no banner yet: those come with ade's own settings file.
static const ed_program ADE = { &ADE_APP, &ADE_KEYS, NULL, NULL };

int main(int argc, char** argv) {
    static editor ed;

    const char* fname = argc > 1 ? argv[1] : NULL;
    if (ed_init_for(&ed, TB_DOC_KB, fname, &ADE) == NULL) {
        return 1;
    }
    ed_run(&ed);
    ed_destroy(&ed);

    return 0;
}
