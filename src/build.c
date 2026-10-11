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
#include <stdio.h>
#include <string.h>

#include <hub/hub.h>

#include "cmd_ops.h"
#include "options.h"
#include "text.h"
#include "project.h"
#include "session.h"
#include "user_input.h"
#include "windows.h"

#define ERR_FILE "/ade.err"

// The screen mode, from MOS's system variables.
#define SYSVAR_SCRMODE 0x27

static int screen_mode(void) {
    return ((volatile unsigned char*) mos_sysvars())[SYSVAR_SCRMODE];
}

// Waiting for a mode change to finish: about three seconds of frames, a
// bound reached only by a VDP that never answers.
#define MODE_FRAMES 180

// How each set_mode went, oldest first -- 'y' settled, 'n' not -- for the
// scripted test.
static char settled[8];
static int nsettled;

// Puts the screen in `mode`, and returns once the change has finished. MOS
// learns the new screen only when the VDP's mode packet arrives, and on
// hardware that is well after the VDU 22: laid out sooner, the screen is laid
// out for the old mode, which shows as a blank text area. The VDP takes
// commands in order, so asking for the mode after the change is answered
// once the change is done -- aed's scr_resume waits the same way.
static void set_mode(int mode) {
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
        settled[nsettled++] = done && screen_mode() == mode ? 'y' : 'n';
    }
}

// What ui_message adds after a message, which has to fit on the row with it:
// text written past the end of the bottom row wraps, and the VDP scrolls the
// whole screen up a line to make room for it.
#define DISMISS_LEN 16                  // " (press any key)"

// Shows `msg` on the message row and waits for a key, cut to fit the row
// beside what ui_message adds -- and marked "..." where it was cut.
static char said[128];

static void say(editor* ed, const char* msg) {
    char* fit = said;
    int room = ed->scr_.barW_ - DISMISS_LEN - 1;
    if (room > (int) sizeof(said) - 1) {
        room = (int) sizeof(said) - 1;
    }
    int n = (int) strlen(msg);
    if (n > room) {
        n = room > 3 ? room - 3 : 0;
        memcpy(fit, msg, (size_t) n);
        memcpy(fit + n, "...", 3);
        n += 3;
    } else {
        memcpy(fit, msg, (size_t) n);
    }
    fit[n] = 0;
    ui_message(&ed->ui_, &ed->scr_, fit);
}

static session* block(void) {
    return (session*) hub_block(SESSION_TAG, sizeof(session));
}

// The session in `s`, if one was written and not yet resumed -- by this ade.
static bool kept(const session* s) {
    return s != NULL && memcmp(s->magic, MAGIC, 4) == 0 && s->size == sizeof(session);
}

// The name without its extension, and which tool builds it -- 'c' for acc,
// 's' for zap, 0 for a file neither does.
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
    if ((strcmp(ext, "s") == 0 && dot[2] == 0)
            || (strcmp(ext, "asm") == 0 && dot[4] == 0)) {
        return 's';
    }

    return 0;
}

// What the build makes, as hub runs it: the file's name or the project's,
// without .bin.
static char program[PROJ_NAME_MAX];

// ...and the command that runs it, for Run.
static char run_cmd[HUB_CMD_MAX + 1];

// The command that builds the file on screen, into cmd; false with a message
// shown when there is none.
static bool file_command(editor* ed, char* cmd) {
    user_input* ui = &ed->ui_;
    screen* scr = &ed->scr_;

    const char* name = tb_fname(&ed->doc_->buf_);
    static char base[TB_FNAME_MAX];
    const char tool = name != NULL ? tool_for(name, base, TB_FNAME_MAX) : 0;
    if (tool == 0) {
        say(ed, "Build what? ade builds .c with acc, .s and .asm with zap");
        return false;
    }

    text t;
    text_start(&t, cmd, HUB_CMD_MAX + 1);
    text_str(&t, tool == 'c' ? "acc " : "zap ");
    text_str(&t, name);
    text_str(&t, tool == 'c' ? " -o " : " ");
    text_str(&t, base);
    text_str(&t, tool == 'c' ? ".bin -errors " ERR_FILE : ".bin -c -e " ERR_FILE);
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

// The command that builds the project, into cmd: Obey on PROJ_OBEY, made
// again if project.ini has changed since it last was. False with a message
// shown when it cannot be.
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
    proj_dirs();
    if (!proj_list(p, msg, (int) sizeof(msg))) {
        say(ed, msg);
        return false;
    }
    text_start(&t, cmd, HUB_CMD_MAX + 1);
    text_str(&t, "Obey " PROJ_OBEY);

    return true;
}

// How the last build went, for the menu bar: "built", "failed", or what the
// program run after it returned; empty until there has been one.
static char status[40];

const char* build_status(void) {
    return status;
}

// The project in the current directory, as last read: at F9, or for the
// header the first time it asks. `loaded` is proj_load's answer, and 2 for
// not read yet.
static project proj;
static int loaded = 2;

const char* build_project(void) {
    if (loaded == 2) {
        static char msg[80];
        loaded = proj_load(&proj, msg, (int) sizeof(msg));
    }

    return loaded > 0 ? proj.name : NULL;
}

const char* build_project_file(void) {
    return build_project() != NULL ? proj.files[0] : NULL;
}

void build_new_project(editor* ed) {
    // The program's name, offered as the file on screen's without its
    // directory or extension: the usual case is a project named for the file
    // holding main.
    static char offer[PROJ_NAME_MAX];
    offer[0] = 0;
    const char* name = tb_fname(&ed->doc_->buf_);
    if (name != NULL) {
        const char* slash = strrchr(name, '/');
        const char* from = slash != NULL ? slash + 1 : name;
        const char* dot = strrchr(from, '.');
        int len = dot != NULL ? (int) (dot - from) : (int) strlen(from);
        if (len >= (int) sizeof(offer)) {
            len = 0;
        }
        memcpy(offer, from, (size_t) len);
        offer[len] = 0;
    }

    char* typed = NULL;
    int sz = 0;
    const RESPONSE got = ui_text(&ed->ui_, &ed->scr_, "Name the program:", offer,
                                 &typed, &sz);
    if (got != YES_OPT || sz <= 0) {
        cmd_restore_after_modal(ed, false);
        return;
    }
    // A name of its own: it is the program's file, name.bin, and %n in the
    // rules, so nothing that would make those something else.
    static char chosen[PROJ_NAME_MAX];
    int bad = sz >= (int) sizeof(chosen);
    for (int i = 0; i < sz; i++) {
        const char c = typed[i];
        if (c <= ' ') {
            bad = 1;
        }
        if (c == '/' || c == '.' || c == ':') {
            bad = 1;
        }
    }
    if (bad) {
        say(ed, "A program's name is one word, with no '/', '.' or ':'");
        return;
    }
    memcpy(chosen, typed, (size_t) sz);
    chosen[sz] = 0;

    static char msg[80];
    if (!proj_create(chosen, msg, (int) sizeof(msg))) {
        say(ed, msg);
        return;
    }
    // Read again, for the header, which names it from now on.
    loaded = 2;
    scr_header_draw(&ed->scr_);
    text t;
    text_start(&t, msg, (int) sizeof(msg));
    text_str(&t, "Project ");
    text_str(&t, chosen);
    text_str(&t, ": CTRL+B builds every .c, .s and .asm here");
    say(ed, msg);
}

// Saves every window that needs it, since what runs next reads the files,
// not ade's windows. A window with no name cannot be, so it stops there; one
// put away has nothing to save: it was saved to be put away. False, having
// said why, when one could not be.
static bool save_windows(editor* ed) {
    for (int i = 0; i < win_count(); i++) {
        document* d = win_doc(ed, i);
        if (d == NULL || !tb_changed(&d->buf_)) {
            continue;
        }
        const char* n = tb_fname(&d->buf_);
        static char msg[48];
        if (n == NULL || n[0] == 0) {
            text t;
            text_start(&t, msg, (int) sizeof(msg));
            text_str(&t, "Window ");
            text_int(&t, i + 1);
            text_str(&t, " has no name: save it first");
            say(ed, msg);
            return false;
        }
        if (!tb_save(&d->buf_)) {
            text t;
            text_start(&t, msg, (int) sizeof(msg));
            text_str(&t, "Window ");
            text_int(&t, i + 1);
            text_str(&t, " could not be saved");
            say(ed, msg);
            return false;
        }
    }

    return true;
}

// Writes ade's windows into hub's block, for ade -resume to bring back: `run`
// for a build that runs its program, `shell` for hub's prompt. NULL, having
// said so, when hub has no room.
static session* keep_windows(editor* ed, bool run, bool shell) {
    session* s = block();
    if (s == NULL) {
        say(ed, "hub has no room to keep ade's windows");
        return NULL;
    }
    memcpy(s->magic, MAGIC, 4);
    s->size = sizeof(session);
    s->mode = (unsigned char) screen_mode();
    s->ran = run;
    s->shell = shell;
    memcpy(s->program, program, sizeof(program));
    s->current = (unsigned char) win_current();
    for (int i = 0; i < ADE_WINDOWS; i++) {
        const char* name = win_name(ed, i);
        s->open[i] = name != NULL && strlen(name) < TB_FNAME_MAX;
        if (!s->open[i]) {
            continue;
        }
        strcpy(s->name[i], name);
        win_where(ed, i, &s->line[i], &s->x[i]);
    }

    return s;
}

// Builds, then with `run` runs what was built, and comes back.
static void start(editor* ed, bool run) {
    status[0] = 0;
    user_input* ui = &ed->ui_;
    screen* scr = &ed->scr_;

    // With a project, the project; without one, the file on screen.
    static char msg[80];
    static char cmd[HUB_CMD_MAX + 1];
    const int has = proj_load(&proj, msg, (int) sizeof(msg));
    loaded = has;
    if (has < 0) {
        say(ed, msg);
        return;
    }
    const bool made = has > 0 ? project_command(ed, &proj, cmd)
                              : file_command(ed, cmd);
    if (!made) {
        return;
    }

    if (!save_windows(ed)) {
        return;
    }
    session* s = keep_windows(ed, run, false);
    if (s == NULL) {
        return;
    }

    // An error left by an earlier build must not be taken for this one's.
    mos_del(ERR_FILE);
    bool queued = hub_enter("ADE ") == HUB_OK
                  && hub_push(cmd, HUB_STOP_ON_ERROR) == HUB_OK;
    // The program after the build -- skipped by hub if a step failed -- on a
    // screen as hub's prompt leaves it, and held for a key after, so what it
    // printed can be read before ade paints over it. Whatever it returns,
    // ade comes back.
    if (queued && run) {
        queued = hub_push(run_cmd, HUB_USER_PROGRAM | HUB_PAUSE_AFTER) == HUB_OK;
    }
    if (!queued || hub_return_to("ade -resume") != HUB_OK) {
        memset(s->magic, 0, 4);
        say(ed, "hub would not take the build");
        return;
    }
    ed->leaving_ = true;
}

void build(editor* ed) {
    start(ed, false);
}

// The shell job's command, which hub 0.6 does not run. A hub older than 0.6
// does not know HUB_SHELL and runs it as a command instead: named so that no
// program answers to it, it fails there -- where "shell" would run a
// shell.bin, if the card had one, and ade take its 0 for the shell's exit.
#define SHELL_JOB "ade-shell-needs-hub-0.6"

void build_shell(editor* ed) {
    if (!save_windows(ed)) {
        return;
    }
    session* s = keep_windows(ed, false, true);
    if (s == NULL) {
        return;
    }
    // hub's prompt, on its own screen, until the user types exit -- the
    // command is not run -- and then ade back. A hub older than 0.6 runs
    // SHELL_JOB as a command instead, which fails, and ade comes back to say
    // so.
    if (hub_enter("ADE ") != HUB_OK
        || hub_push(SHELL_JOB, HUB_SHELL | HUB_USER_PROGRAM) != HUB_OK
        || hub_return_to("ade -resume") != HUB_OK) {
        memset(s->magic, 0, 4);
        say(ed, "hub would not open its shell");
        return;
    }
    ed->leaving_ = true;
}

void build_run(editor* ed) {
    start(ed, true);
}

void build_user_screen(editor* ed) {
    const int mode = hub_user_screen();
    if (mode < 0) {
        say(ed, "No program has run yet: CTRL+F9 runs one");
        return;
    }
    const int was = screen_mode();

    // In the mode the program left, if that is another; then logical
    // coordinates from the bottom left, as the capture was taken, and the
    // buffer hub put it in drawn at 0,0.
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

    // Back to ade's mode, which takes the font and colours with it, so ade's
    // settings go back on before the screen is painted again.
    if (mode != was) {
        set_mode(was);
        ade_settings(ed);
    }
    scr_footer_invalidate(&ed->scr_);
    cmd_restore_after_modal(ed, false);
}

void build_resume_screen(void) {
    const session* s = block();
    if (!kept(s) || s->mode == screen_mode()) {
        return;
    }
    set_mode(s->mode);
}

const char* build_first_file(void) {
    session* s = block();
    if (!kept(s) || !s->open[0]) {
        return NULL;
    }

    return s->name[0][0] != 0 ? s->name[0] : NULL;
}

// Puts window w's cursor at (line, x) and shows it there: one paint, whether
// it was on screen already or not.
static void show_at(editor* ed, int w, int line, int x) {
    win_place(ed, w, line, x);
    if (win_current() != w) {
        win_go(ed, w);
    } else {
        cmd_restore_after_modal(ed, false);
    }
}

// What the last build said: the error file, each line a string, and where
// each starts -- the errors, most of the form `file:line:column: ...`, but
// some with no place in a file to give. Kept until the next build, for the
// list.
#define ERR_MAX 16
static char errs[1024];
static int err_at[ERR_MAX];
static int nerrs;
static int chosen;

// The position in an error line: the file into `file`, the line and column
// into *line and *col. The file may hold a drive's colon, so the numbers are
// found from the first ':' followed by a digit.
static bool parse_error(const char* s, char* file, int fmax, int* line, int* col) {
    const char* c1 = NULL;
    for (const char* q = s; *q != 0; q++) {
        if (q[0] == ':' && q[1] >= '0' && q[1] <= '9') {
            c1 = q;
            break;
        }
    }
    if (c1 == NULL || c1 == s || c1 - s >= fmax) {
        return false;
    }
    memcpy(file, s, (size_t) (c1 - s));
    file[c1 - s] = 0;
    *line = 0;
    *col = 0;
    const char* q = c1 + 1;
    while (*q >= '0' && *q <= '9') {
        *line = *line * 10 + (*q++ - '0');
    }
    if (*q == ':') {
        q++;
        while (*q >= '0' && *q <= '9') {
            *col = *col * 10 + (*q++ - '0');
        }
    }

    return *line > 0;
}

// What an error says, for the message row: from its line and column on --
// `line:column: ...` -- since its file is the one on screen, named in the
// footer. One at line 0, which is in no file, says only what is past them.
static const char* error_text(const char* e) {
    const char* q = e;
    while (*q != 0 && !(q[0] == ':' && q[1] >= '0' && q[1] <= '9')) {
        q++;
    }
    if (*q == 0) {
        return e;                       // not that shape: all of it
    }
    const char* place = q + 1;
    bool nowhere = true;
    for (int field = 0; field < 2 && *q == ':'; field++) {
        q++;
        while (*q >= '0' && *q <= '9') {
            nowhere = nowhere && (field > 0 || *q == '0');
            q++;
        }
    }
    if (*q != ':') {
        return e;
    }
    if (!nowhere) {
        return place;
    }
    q++;
    while (*q == ' ') {
        q++;
    }

    return *q != 0 ? q : e;
}

// Reads the error file into errs, and finds the errors in it.
static void read_errors(void) {
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

    int start = 0;
    while (start < n && nerrs < ERR_MAX) {
        int end = start;
        while (end < n && errs[end] != '\r' && errs[end] != '\n') {
            end++;
        }
        errs[end] = 0;
        // Every line is an error, whether it says where or not: a link's
        // "'add' is called but never defined" has no place in a file, and
        // acc writes it as at acc:0:0.
        if (errs[start] != 0) {
            err_at[nerrs++] = start;
        }
        start = end + 1;
        while (start < n && (errs[start] == '\r' || errs[start] == '\n')) {
            start++;
        }
    }
}

// Where error k is: its window -- opened if its file has none -- and the
// cursor's line and x there, the tools' column being a byte in the line from
// 1, with 0 for not known. The window is -1 when it cannot be opened.
static int error_window(editor* ed, int k, int* line, int* x) {
    static char file[TB_FNAME_MAX];
    int col = 0;
    // An error with no line in a file has nowhere to go: the cursor stays.
    if (!parse_error(errs + err_at[k], file, TB_FNAME_MAX, line, &col)) {
        return -1;
    }
    *x = col > 0 ? col - 1 : 0;
    const int w = win_find(ed, file);

    return w >= 0 ? w : win_open_named(ed, file);
}

// Goes to error k and shows what it says.
static void go_to_error(editor* ed, int k) {
    int line = 0;
    int x = 0;
    const int w = error_window(ed, k, &line, &x);
    if (w >= 0) {
        show_at(ed, w, line, x);
    }
    chosen = k;
    say(ed, error_text(errs + err_at[k]));
}

void build_errors(editor* ed) {
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
            go_to_error(ed, at);
            return;
        }
    }
}

void build_resume(editor* ed) {
    session* s = block();
    if (!kept(s)) {
        return;
    }
    memset(s->magic, 0, 4);             // used: a later start is not a resume

    // Window 1 is open already: ed_init_for opened its file. The others, in
    // order, put away -- each is loaded when it is shown -- where they were.
    // One whose name is too long to keep put away is loaded now instead.
    for (int i = 1; i < ADE_WINDOWS; i++) {
        if (s->open[i] && win_restore(ed, s->name[i], s->line[i], s->x[i]) < 0) {
            const int w = win_open_named(ed, s->name[i]);
            if (w >= 0) {
                win_place(ed, w, s->line[i], s->x[i]);
            }
        }
    }

    // Where ade ends up: the window that was on screen, as it was -- or, when
    // the build failed, the error's file at the error. A paint costs a fifth
    // of a second or so, so that is the only window painted.
    int target = s->open[s->current] ? win_find(ed, s->name[s->current]) : -1;
    if (target < 0) {
        target = 0;
    }
    int line = s->line[s->current];
    int x = s->x[s->current];

    // Back from hub's shell: no build to say anything about. Its job ends
    // with 0 at exit; anything else is a hub that does not know HUB_SHELL.
    if (s->shell) {
        for (int i = 0; i < ADE_WINDOWS; i++) {
            const int w = s->open[i] ? win_find(ed, s->name[i]) : -1;
            if (w >= 0 && w != target) {
                win_place(ed, w, s->line[i], s->x[i]);
            }
        }
        show_at(ed, target, line, x);
        if (hub_last_result() != 0) {
            say(ed, "The shell needs hub 0.6 or later");
        }
        return;
    }
    const bool failed = hub_failed_job() >= 0;
    if (failed) {
        read_errors();
    }
    if (nerrs > 0) {
        int eline = 0;
        int ex = 0;
        const int e = error_window(ed, 0, &eline, &ex);
        if (e >= 0) {
            target = e;
            line = eline;
            x = ex;
        }
    }
    // Every other window placed without drawing -- each is painted when it
    // is shown -- and then the one ade ends up on, shown there: one paint for
    // the lot.
    for (int i = 0; i < ADE_WINDOWS; i++) {
        const int w = s->open[i] ? win_find(ed, s->name[i]) : -1;
        if (w >= 0 && w != target) {
            win_place(ed, w, s->line[i], s->x[i]);
        }
    }
    // How it went, on the menu bar until the next build: drawn with the
    // screen, so before it is shown.
    text t;
    text_start(&t, status, (int) sizeof(status));
    if (!failed && s->ran) {
        text_strn(&t, s->program, 24);
        text_str(&t, " returned ");
        text_int(&t, hub_last_result());
    } else {
        text_str(&t, failed ? "failed" : "built");
    }
    show_at(ed, target, line, x);

    // A build that worked goes straight back to the editor: the bar says so,
    // and there is nothing to read that needs a key to get past. One that
    // failed says why, which waits; with more than one error, the list, from
    // which each can be gone to.
    if (!failed) {
        return;
    }
    if (nerrs == 0) {
        static char msg[40];
        text_start(&t, msg, (int) sizeof(msg));
        text_str(&t, "The build failed, with ");
        text_int(&t, hub_last_result());
        say(ed, msg);
    } else if (nerrs == 1) {
        say(ed, error_text(errs + err_at[0]));
    } else {
        build_errors(ed);
    }
}

const char* build_said(void) {
    return said;
}

const char* build_modes_settled(void) {
    return settled;
}
