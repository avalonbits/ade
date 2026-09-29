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

#include <hub.h>

#include "cmd_ops.h"
#include "user_input.h"
#include "windows.h"

#define ERR_FILE "/ade.err"

// What survives the build, in hub's block "ADE ": which files were open, in
// which windows, where each cursor was, and which window was on screen.
typedef struct {
    char magic[4];                      // "ADE1" once written
    unsigned char open[ADE_WINDOWS];
    unsigned char current;
    int line[ADE_WINDOWS];
    int x[ADE_WINDOWS];
    char name[ADE_WINDOWS][TB_FNAME_MAX];
} session;

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

void build(editor* ed) {
    user_input* ui = &ed->ui_;
    screen* scr = &ed->scr_;

    const char* name = tb_fname(&ed->doc_->buf_);
    static char base[TB_FNAME_MAX];
    const char tool = name != NULL ? tool_for(name, base, TB_FNAME_MAX) : 0;
    if (tool == 0) {
        ui_message(ui, scr, "Build what? ade builds .c with acc, .s and .asm with zap");
        return;
    }

    static char cmd[2 * TB_FNAME_MAX + 40];
    if (tool == 'c') {
        snprintf(cmd, sizeof(cmd), "acc %s -o %s.bin -errors " ERR_FILE, name, base);
    } else {
        snprintf(cmd, sizeof(cmd), "zap %s %s.bin -c -e " ERR_FILE, name, base);
    }
    if ((int) strlen(cmd) > HUB_CMD_MAX) {
        ui_message(ui, scr, "The file's name is too long for hub to run the build");
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
    memcpy(s->magic, "ADE1", 4);
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
    if (hub_enter("ADE ") != HUB_OK
            || hub_push(cmd, HUB_STOP_ON_ERROR) != HUB_OK
            || hub_return_to("ade -resume") != HUB_OK) {
        memset(s->magic, 0, 4);
        ui_message(ui, scr, "hub would not take the build");
        return;
    }
    ed->leaving_ = true;
}

const char* build_first_file(void) {
    session* s = block();
    if (s == NULL || memcmp(s->magic, "ADE1", 4) != 0 || !s->open[0]) {
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

// The first line of the error file, as `file:line:column: ...`: the file into
// `file`, the position into *line and *col, the whole line into `text`.
static bool read_error(char* file, int fmax, int* line, int* col,
                       char* text, int tmax) {
    const char fh = mos_fopen(ERR_FILE, FA_READ);
    if (fh == 0) {
        return false;
    }
    static char buf[256];
    const int n = (int) mos_fread(fh, buf, sizeof(buf) - 1);
    mos_fclose(fh);
    if (n <= 0) {
        return false;
    }
    buf[n] = 0;
    char* end = strpbrk(buf, "\r\n");
    if (end != NULL) {
        *end = 0;
    }
    snprintf(text, (size_t) tmax, "%s", buf);

    // file:line:col: -- the file may hold a drive's colon, so the numbers are
    // found from the first ':' followed by a digit.
    char* p = buf;
    char* c1 = NULL;
    for (char* q = buf; *q != 0; q++) {
        if (q[0] == ':' && q[1] >= '0' && q[1] <= '9') {
            c1 = q;
            break;
        }
    }
    if (c1 == NULL || c1 - p >= fmax) {
        return false;
    }
    memcpy(file, p, (size_t) (c1 - p));
    file[c1 - p] = 0;
    *line = 0;
    *col = 0;
    char* q = c1 + 1;
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

void build_resume(editor* ed) {
    session* s = block();
    if (s == NULL || memcmp(s->magic, "ADE1", 4) != 0) {
        return;
    }
    // Window 1 is open already: ed_init_for opened its file. The others, in
    // order, then each cursor where it was.
    for (int i = 1; i < ADE_WINDOWS; i++) {
        if (s->open[i]) {
            win_open_named(ed, s->name[i]);
        }
    }
    for (int i = 0; i < ADE_WINDOWS; i++) {
        const int w = s->open[i] ? win_find(ed, s->name[i]) : -1;
        if (w >= 0) {
            win_go(ed, w);
            place(ed, s->line[i], s->x[i]);
        }
    }
    const int back = s->open[s->current] ? win_find(ed, s->name[s->current]) : -1;
    win_go(ed, back >= 0 ? back : 0);
    memset(s->magic, 0, 4);             // used: a later start is not a resume

    if (hub_failed_job() < 0) {
        ui_message(&ed->ui_, &ed->scr_, "Built");
        cmd_restore_after_modal(ed, false);
        return;
    }
    static char file[TB_FNAME_MAX];
    static char text[128];
    int line = 0;
    int col = 0;
    if (!read_error(file, TB_FNAME_MAX, &line, &col, text, (int) sizeof(text))) {
        static char msg[40];
        snprintf(msg, sizeof(msg), "The build failed, with %d", hub_last_result());
        ui_message(&ed->ui_, &ed->scr_, msg);
        cmd_restore_after_modal(ed, false);
        return;
    }
    // To the error: its file's window, opened if it has none; the column is
    // the tools' -- a tab is one -- which is a byte in the line, from 1, with
    // 0 for not known.
    int w = win_find(ed, file);
    if (w < 0) {
        w = win_open_named(ed, file);
    }
    if (w >= 0) {
        win_go(ed, w);
        place(ed, line, col > 0 ? col - 1 : 0);
    }
    ui_message(&ed->ui_, &ed->scr_, text);
    cmd_restore_after_modal(ed, false);
}
