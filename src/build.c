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
#include "project.h"
#include "user_input.h"
#include "windows.h"

#define ERR_FILE "/ade.err"

// What survives the build, in hub's block "ADE ": which files were open, in
// which windows, where each cursor was, and which window was on screen.
typedef struct {
    char magic[4];                      // MAGIC once written
    bool ran;                           // the program was queued after the build
    char program[PROJ_NAME_MAX];        // ...and what it is called
    unsigned char open[ADE_WINDOWS];
    unsigned char current;
    int line[ADE_WINDOWS];
    int x[ADE_WINDOWS];
    char name[ADE_WINDOWS][TB_FNAME_MAX];
} session;

// Changed whenever the session does, so an older ade's is not misread.
#define MAGIC "ADE2"

static session* block(void) {
    return (session*) hub_block("ADE ", sizeof(session));
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

// The command that builds the file on screen, into cmd; false with a message
// shown when there is none.
static bool file_command(editor* ed, char* cmd) {
    user_input* ui = &ed->ui_;
    screen* scr = &ed->scr_;

    const char* name = tb_fname(&ed->doc_->buf_);
    static char base[TB_FNAME_MAX];
    const char tool = name != NULL ? tool_for(name, base, TB_FNAME_MAX) : 0;
    if (tool == 0) {
        ui_message(ui, scr, "Build what? ade builds .c with acc, .s and .asm with zap");
        return false;
    }

    const int n = tool == 'c'
        ? snprintf(cmd, HUB_CMD_MAX + 1, "acc %s -o %s.bin -errors " ERR_FILE, name, base)
        : snprintf(cmd, HUB_CMD_MAX + 1, "zap %s %s.bin -c -e " ERR_FILE, name, base);
    if (n > HUB_CMD_MAX) {
        ui_message(ui, scr, "The file's name is too long for hub to run the build");
        return false;
    }
    if (snprintf(program, sizeof(program), "%s", base) >= (int) sizeof(program)) {
        ui_message(ui, scr, "The file's name is too long for hub to run the program");
        return false;
    }

    return true;
}

// The command that builds the project, into cmd: Obey on PROJ_OBEY, made
// again if project.ini has changed since it last was. False with a message
// shown when it cannot be.
static bool project_command(editor* ed, const project* p, char* cmd) {
    snprintf(program, sizeof(program), "%s", p->name);
    static char msg[80];
    if (!proj_obey(p, ERR_FILE, msg, (int) sizeof(msg))) {
        ui_message(&ed->ui_, &ed->scr_, msg);
        return false;
    }
    snprintf(cmd, HUB_CMD_MAX + 1, "Obey " PROJ_OBEY);

    return true;
}

// Builds, then with `run` runs what was built, and comes back.
static void start(editor* ed, bool run) {
    user_input* ui = &ed->ui_;
    screen* scr = &ed->scr_;

    // With a project, the project; without one, the file on screen.
    static project proj;
    static char msg[80];
    static char cmd[HUB_CMD_MAX + 1];
    const int has = proj_load(&proj, msg, (int) sizeof(msg));
    if (has < 0) {
        ui_message(ui, scr, msg);
        return;
    }
    const bool made = has > 0 ? project_command(ed, &proj, cmd)
                              : file_command(ed, cmd);
    if (!made) {
        return;
    }

    // Everything that needs saving is saved first: the tool reads the files,
    // not ade's windows. A window with no name cannot be, so it stops here.
    for (int i = 0; i < ADE_WINDOWS; i++) {
        document* d = win_doc(ed, i);
        if (d == NULL || !tb_changed(&d->buf_)) {
            continue;
        }
        const char* n = tb_fname(&d->buf_);
        if (n == NULL || n[0] == 0) {
            static char msg[] = "Window 1 has no name: save it first";
            msg[7] = (char) ('1' + i);
            ui_message(ui, scr, msg);
            return;
        }
        if (!tb_save(&d->buf_)) {
            static char msg[] = "Window 1 could not be saved";
            msg[7] = (char) ('1' + i);
            ui_message(ui, scr, msg);
            return;
        }
    }

    session* s = block();
    if (s == NULL) {
        ui_message(ui, scr, "hub has no room to keep ade's windows");
        return;
    }
    memcpy(s->magic, MAGIC, 4);
    s->ran = run;
    memcpy(s->program, program, sizeof(program));
    s->current = (unsigned char) win_current();
    for (int i = 0; i < ADE_WINDOWS; i++) {
        document* d = win_doc(ed, i);
        s->open[i] = d != NULL;
        if (d == NULL) {
            continue;
        }
        snprintf(s->name[i], TB_FNAME_MAX, "%s", tb_fname(&d->buf_));
        s->line[i] = tb_ypos(&d->buf_);
        s->x[i] = tb_xpos(&d->buf_) - 1;
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
        queued = hub_push(program, HUB_USER_PROGRAM | HUB_PAUSE_AFTER) == HUB_OK;
    }
    if (!queued || hub_return_to("ade -resume") != HUB_OK) {
        memset(s->magic, 0, 4);
        ui_message(ui, scr, "hub would not take the build");
        return;
    }
    ed->leaving_ = true;
}

void build(editor* ed) {
    start(ed, false);
}

void build_run(editor* ed) {
    start(ed, true);
}

const char* build_first_file(void) {
    session* s = block();
    if (s == NULL || memcmp(s->magic, MAGIC, 4) != 0 || !s->open[0]) {
        return NULL;
    }

    return s->name[0][0] != 0 ? s->name[0] : NULL;
}

// Puts a document's cursor at (line, x) and the view round it.
static void place(editor* ed, int line, int x) {
    tb_seek(&ed->doc_->buf_, (tb_pos) { .line = line, .x = x });
    tb_settle(&ed->doc_->buf_);
    cmd_restore_after_modal(ed, true);
}

// What the last build said: the error file, each line a string, and where
// the lines of the form `file:line:column: ...` start -- the errors. Kept
// until the next build, for the list.
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

    static char file[TB_FNAME_MAX];
    int start = 0;
    while (start < n && nerrs < ERR_MAX) {
        int end = start;
        while (end < n && errs[end] != '\r' && errs[end] != '\n') {
            end++;
        }
        errs[end] = 0;
        int line = 0;
        int col = 0;
        if (parse_error(errs + start, file, TB_FNAME_MAX, &line, &col)) {
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
    parse_error(errs + err_at[k], file, TB_FNAME_MAX, line, &col);
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
        if (win_current() != w) {
            win_go(ed, w);
        }
        place(ed, line, x);
    }
    chosen = k;
    ui_message(&ed->ui_, &ed->scr_, errs + err_at[k]);
}

void build_errors(editor* ed) {
    if (nerrs == 0) {
        ui_message(&ed->ui_, &ed->scr_, "The last build left no errors");
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
    if (s == NULL || memcmp(s->magic, MAGIC, 4) != 0) {
        return;
    }
    memset(s->magic, 0, 4);             // used: a later start is not a resume

    // Window 1 is open already: ed_init_for opened its file. The others, in
    // order.
    for (int i = 1; i < ADE_WINDOWS; i++) {
        if (s->open[i]) {
            win_open_named(ed, s->name[i]);
        }
    }

    // Where ade ends up: the window that was on screen, as it was -- or, when
    // the build failed, the error's file at the error. Every paint costs a
    // fifth of a second or so, and each place() is one, so that window is
    // placed once, last, and only the others are shown on the way.
    int target = s->open[s->current] ? win_find(ed, s->name[s->current]) : -1;
    if (target < 0) {
        target = 0;
    }
    int line = s->line[s->current];
    int x = s->x[s->current];
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
    for (int i = 0; i < ADE_WINDOWS; i++) {
        const int w = s->open[i] ? win_find(ed, s->name[i]) : -1;
        if (w >= 0 && w != target) {
            win_go(ed, w);
            place(ed, s->line[i], s->x[i]);
        }
    }
    if (win_current() != target) {
        win_go(ed, target);
    }
    place(ed, line, x);

    // A message is on the footer, which the loop repaints after it; with more
    // than one error, the list, from which each can be gone to.
    if (!failed && s->ran) {
        static char msg[PROJ_NAME_MAX + 20];
        snprintf(msg, sizeof(msg), "%s returned %d", s->program, hub_last_result());
        ui_message(&ed->ui_, &ed->scr_, msg);
    } else if (!failed) {
        ui_message(&ed->ui_, &ed->scr_, "Built");
    } else if (nerrs == 0) {
        static char msg[40];
        snprintf(msg, sizeof(msg), "The build failed, with %d", hub_last_result());
        ui_message(&ed->ui_, &ed->scr_, msg);
    } else if (nerrs == 1) {
        ui_message(&ed->ui_, &ed->scr_, errs + err_at[0]);
    } else {
        build_errors(ed);
    }
}
