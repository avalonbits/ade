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
 * ade opens the file named on the command line and edits it as aed does, with
 * up to three files open in windows of their own -- see windows.h. Its own
 * keys are its files, its windows and leaving; every other key is the
 * editor's, from ED_KEYS behind them.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <agon/mos.h>
#include <hub/hub.h>

#include "app.h"
#include "build.h"
#include "options.h"
#include "editor.h"
#include "menu.h"
#include "text_buffer.h"
#include "windows.h"

#ifdef ADE_SCRIPT
#include "script.h"
#endif

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
#define A MOD_ALT
#define S MOD_SHFT

// Both cases of a letter: MOS reports the shifted one when SHIFT is held.
#define LETTER(lo, up, mods, cmd) \
    { lo, mods, 0, cmd }, { up, mods, 0, cmd }

/*
 * ade's own keys: its files, leaving and its settings (CTRL+E), the same as
 * aed's; CTRL+SHIFT+O opens a file in a new window; and its windows and menus
 * on the keys Turbo Pascal gave them -- F6 goes to the next window, ALT+F3
 * closes the one on screen, ALT+F6 lists them, and F10 opens the menus. F9
 * builds, through hub -- see build.h -- ALT+F9 lists the errors it left,
 * CTRL+F9 builds and runs, and ALT+F5 shows what the program left on the
 * screen. A key with more held comes before the same key with fewer, which
 * would match it too: CTRL+SHIFT+O before CTRL+O, ALT+F6 before F6, and
 * ALT+F9 and CTRL+F9 before F9.
 */
static const key_binding ADE_BINDINGS[] = {
    LETTER(VK_o, VK_O, C | S, win_open),
    LETTER(VK_o, VK_O, C, cmd_open),
    LETTER(VK_s, VK_S, C | A, cmd_save_as),
    LETTER(VK_s, VK_S, C, ed_cmd_save),
    LETTER(VK_q, VK_Q, C, win_quit),
    LETTER(VK_e, VK_E, C, ade_cmd_settings),
    { VK_F3, A, 0, win_close },
    { VK_F6, A, 0, win_list },
    { VK_F6, 0, 0, win_next },
    // The menus own the selection, so one made before opening them is still
    // there for Copy or Cut.
    { VK_F10, 0, KC_OWNS_SEL, menu_open },
    { VK_F5, A, 0, build_user_screen },
    { VK_F9, A, 0, build_errors },
    { VK_F9, C, 0, build_run },
    { VK_F9, 0, 0, build },
};

#undef LETTER
#undef S
#undef A
#undef C

static const keymap ADE_KEYS = {
    ADE_BINDINGS, (int) (sizeof(ADE_BINDINGS) / sizeof(ADE_BINDINGS[0])),
    &ED_KEYS,
};

// The editor's settings, in ade's own file -- see options.h. No banner yet.
// What ade sets up as the screen comes into being, before the first paint:
// the menu bar's row under the title, then the settings.
static const char* ade_screen(editor* ed) {
    menu_attach(ed);

    return ade_settings(ed);
}

static const ed_program ADE = {
    &ADE_APP, &ADE_KEYS, ade_screen, NULL, "ade: the Agon Development Environment",
};

int main(int argc, char** argv) {
    static editor ed;
#ifdef ADE_SCRIPT
    script_begin();
#endif

    // Building runs the tools through hub, and ade comes back through it; a
    // machine without hub -- or a MOS older than 3.0.2, which hub needs --
    // gets told so rather than an editor that cannot do what it is for.
    if (!hub_present()) {
        static const char why[] =
            "ade runs under hub, which needs MOS 3.0.2 or later.\r\n"
            "Put hub.bin in /mos, type hub, then ade.\r\n";
        mos_puts((char*) why, sizeof(why) - 1, 0);

        return 1;
    }

    // `ade -resume` is hub bringing ade back from a build: the session it
    // left says which files to open.
    const bool resuming = argc > 1 && strcmp(argv[1], "-resume") == 0;
    if (resuming) {
        build_resume_screen();
    }
    const char* fname = resuming ? build_first_file()
                                 : (argc > 1 ? argv[1] : NULL);
    if (ed_init_for(&ed, ADE_DOC_KB, fname, &ADE) == NULL) {
        return 1;
    }
    win_start(&ed);
    menu_start(&ed);
#ifdef ADE_SCRIPT
    // Before the resume: what it says waits for a key.
    ui_set_keys(&ed.ui_, script_keys(&ed, argc > 1 ? argv[1] : NULL));
#endif
    if (resuming) {
        build_resume(&ed);
    }
    ed_run(&ed);
#ifdef ADE_SCRIPT
    script_report(&ed);
#endif
    win_finish(&ed);
    ed_destroy(&ed);

    return 0;
}
