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

#include "build.h"

#include <agon/mos.h>
#include <stdlib.h>
#include <string.h>

#include <hub/hub.h>

#include "away.h"
#include "cmd_ops.h"
#include "errors.h"
#include "mode.h"
#include "options.h"
#include "project.h"
#include "say.h"
#include "text.h"
#include "windows.h"

// What the build makes, as hub runs it -- the file's name or the project's,
// without .bin -- and the command that runs it, for CTRL+R.
static char program[PROJ_NAME_MAX];
static char run_cmd[HUB_CMD_MAX + 1];

// The menu bar's "built", "failed" or "demo returned 0".
static char status[40];

// The name without its extension into `base`, and which tool builds it: 'c'
// for acc, 's' for zap, 0 for neither.
static char tool_for(const char* name, char* base, int max) {
    const char* dot = strrchr(name, '.');
    const char* slash = strrchr(name, '/');
    if (dot == NULL || (slash != NULL && dot < slash)) {
        return 0;
    }
    const int n = (int) (dot - name);
    if (n <= 0 || n >= max) {
        return 0;
    }
    memcpy(base, name, (size_t) n);
    base[n] = 0;

    char ext[5] = { 0 };
    for (int i = 0; i < 4 && dot[1 + i] != 0; i++) {
        const char c = dot[1 + i];
        ext[i] = (char) (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    if (strcmp(ext, "c") == 0 && dot[2] == 0) {
        return 'c';
    }
    if ((strcmp(ext, "s") == 0 && dot[2] == 0) || (strcmp(ext, "asm") == 0 && dot[4] == 0)) {
        return 's';
    }

    return 0;
}

// The command that builds the file on screen, into cmd. False, having said
// why, when there is none.
static bool file_command(editor* ed, char* cmd) {
    const char* name = tb_fname(&ed->doc_->buf_);
    static char base[TB_FNAME_MAX];
    const char tool = name != NULL ? tool_for(name, base, TB_FNAME_MAX) : 0;
    if (tool == 0) {
        say(ed, "Build what? ade builds .c with acc, .s and .asm with zap");
        return false;
    }

    text t;
    text_start(&t, cmd, HUB_CMD_MAX + 1);
    if (tool == 'c') {
        text_str(&t, "acc ");
        text_str(&t, name);
        text_str(&t, " -o ");
        text_str(&t, base);
        text_str(&t, ".bin -errors " ERR_FILE);
    } else {
        text_str(&t, "zap ");
        text_str(&t, name);
        text_str(&t, " ");
        text_str(&t, base);
        text_str(&t, ".bin -c -e " ERR_FILE);
    }
    if (t.cut) {
        say(ed, "The file's name is too long for hub to run the build");
        return false;
    }
    text_start(&t, program, (int) sizeof(program));
    text_str(&t, base);
    if (t.cut) {
        say(ed, "The file's name is too long for hub to run the program");
        return false;
    }
    text_start(&t, run_cmd, (int) sizeof(run_cmd));
    text_str(&t, base);

    return true;
}

// The command that builds the project, into cmd: Obey on build.obey, written
// again first if project.ini has changed. False, having said why, when it
// cannot be.
static bool project_command(editor* ed, const project* p, char* cmd) {
    text t;
    text_start(&t, program, (int) sizeof(program));
    text_str(&t, p->name);
    if (!proj_run(p, run_cmd, (int) sizeof(run_cmd))) {
        say(ed, "run in " PROJ_FILE " is too long for hub");
        return false;
    }
    static char msg[80];
    if (!proj_obey(p, ERR_FILE, msg, (int) sizeof(msg))) {
        say(ed, msg);
        return false;
    }
    proj_dirs(p);
    if (!proj_list(p, msg, (int) sizeof(msg)) || !proj_docs_list(p, msg, (int) sizeof(msg))) {
        say(ed, msg);
        return false;
    }
    text_start(&t, cmd, HUB_CMD_MAX + 1);
    text_str(&t, "Obey " PROJ_OBEY);

    return true;
}

// Builds -- the project here, or else the file on screen -- then, with `run`,
// runs what was built; and comes back.
static void start(editor* ed, bool run) {
    status[0] = 0;

    static char msg[80];
    static char cmd[HUB_CMD_MAX + 1];
    int has = 0;
    project* p = proj_read(&has, msg, (int) sizeof(msg));
    if (p == NULL || has < 0) {
        free(p);
        say(ed, msg);
        return;
    }
    const bool made = has > 0 ? project_command(ed, p, cmd) : file_command(ed, cmd);
    free(p);
    if (!made || !away_save_windows(ed)) {
        return;
    }
    session* s = away_keep(ed, SESSION_BUILD);
    if (s == NULL) {
        return;
    }
    s->ran = run;
    memcpy(s->program, program, sizeof(program));

    errors_clear();
    // A step that fails stops the build, and the program after it is skipped.
    // The program runs on the screen hub's prompt has, and hub waits for a key
    // after it, so what it printed can be read. Whatever it returns, ade comes
    // back.
    away_hand_over(ed, s, cmd, HUB_STOP_ON_ERROR,
                   run ? run_cmd : NULL, HUB_USER_PROGRAM | HUB_PAUSE_AFTER, "take the build");
}

void build(editor* ed) {
    start(ed, false);
}

void build_run(editor* ed) {
    start(ed, true);
}

void build_user_screen(editor* ed) {
    const int mode = hub_user_screen();
    if (mode < 0) {
        say(ed, "No program has run yet: CTRL+R runs one");
        return;
    }
    const int was = mode_now();

    // In the program's mode, if that is another; logical coordinates from the
    // bottom left, as the capture was taken; and hub's buffer drawn at 0,0.
    static char vdu[] = {
        22, 0,                                      // MODE, when it differs
        23, 0, (char) 0xC0, 1,                      // logical coordinates
        29, 0, 0, 0, 0,                             // origin 0,0
        23, 27, 0x20, HUB_SCREEN_BUFFER & 0xFF, HUB_SCREEN_BUFFER >> 8,
        23, 27, 3, 0, 0, 0, 0,                      // draw it at 0,0
    };
    vdu[1] = (char) mode;
    const int skip = mode != was ? 0 : 2;
    mos_puts(vdu + skip, (unsigned) (sizeof(vdu) - skip), 0);

    ks_wait(ed->ui_.keys_);

    // A mode change drops ade's font and colours: they go back on first.
    if (mode != was) {
        mode_set(was);
        ade_settings(ed);
    }
    scr_footer_invalidate(&ed->scr_);
    cmd_restore_after_modal(ed, false);
}

const char* build_status(void) {
    return status;
}

void build_landing(editor* ed, const session* s, int* window, int* line, int* x) {
    const bool failed = hub_failed_job() >= 0;
    if (failed) {
        errors_read();
    }
    if (errors_count() > 0) {
        int eline = 0;
        int ex = 0;
        const int w = errors_window(ed, 0, &eline, &ex);
        if (w >= 0) {
            *window = w;
            *line = eline;
            *x = ex;
        }
    }

    text t;
    text_start(&t, status, (int) sizeof(status));
    if (!failed && s->ran) {
        text_strn(&t, s->program, 24);
        text_str(&t, " returned ");
        text_int(&t, hub_last_result());
    } else {
        text_str(&t, failed ? "failed" : "built");
    }
}

void build_report(editor* ed, const session* s) {
    (void) s;

    // A build that worked has nothing to say that needs a key: the menu bar
    // says "built".
    if (hub_failed_job() < 0) {
        return;
    }
    if (errors_count() == 0) {
        static char msg[40];
        text t;
        text_start(&t, msg, (int) sizeof(msg));
        text_str(&t, "The build failed, with ");
        text_int(&t, hub_last_result());
        say(ed, msg);
    } else if (errors_count() == 1) {
        say(ed, errors_text(0));
    } else {
        errors_list(ed);
    }
}
