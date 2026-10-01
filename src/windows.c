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

#include "windows.h"

#include <agon/mos.h>
#include <string.h>

#include "cmd_ops.h"
#include "user_input.h"

// ade's own state, kept here rather than in the editor, which knows nothing of
// windows. There is one ade, and one set of windows, so it is file-scope.
static document docs[ADE_WINDOWS];      // [0] unused: window 1 is the editor's
static view views[ADE_WINDOWS];         // [0] unused: window 1 has whole_
static bool open_[ADE_WINDOWS];
static int current;

document* win_doc(editor* ed, int i) {
    if (i < 0 || i >= ADE_WINDOWS || !open_[i]) {
        return NULL;
    }

    return i == 0 ? &ed->home_ : &docs[i];
}

static view* win_view(editor* ed, int i);

void win_place(editor* ed, int i, int line, int x) {
    if (win_doc(ed, i) != NULL) {
        ed_doc_place(ed, win_doc(ed, i), win_view(ed, i), line, x);
    }
}

static view* win_view(editor* ed, int i) {
    return i == 0 ? &ed->scr_.whole_ : &views[i];
}

int win_strip(char* out, int max) {
    int n = 0;
    for (int i = 0; i < ADE_WINDOWS && n + 5 < max; i++) {
        if (!open_[i]) {
            continue;
        }
        if (n > 0) {
            out[n++] = ' ';
        }
        if (i == current) {
            out[n++] = '[';
        }
        out[n++] = (char) ('1' + i);
        if (i == current) {
            out[n++] = ']';
        }
    }
    out[n] = 0;

    return n;
}

int win_current(void) {
    return current;
}

void win_start(editor* ed) {
    (void) ed;
    memset(open_, 0, sizeof(open_));
    open_[0] = true;
    current = 0;
}

void win_finish(editor* ed) {
    ed_doc_show(ed, &ed->home_, &ed->scr_.whole_);
    for (int i = 1; i < ADE_WINDOWS; i++) {
        if (open_[i]) {
            ed_doc_close(&docs[i]);
            open_[i] = false;
        }
    }
    current = 0;
}

/*
 * A name as one spelling per file: MOS resolves a relative name against the
 * current directory, so from the root "a.txt", "/a.txt" and "./a.txt" are one
 * file. Put under the current directory, a drive prefix dropped, "." and ".."
 * folded away -- as aed's doc_store does for its scratch files. Empty when it
 * cannot be worked out.
 */
static void resolve(char* out, int max, const char* path) {
    static char joined[2 * TB_FNAME_MAX];
    int j = 0;
    out[0] = 0;
    if (path == NULL || path[0] == 0) {
        return;
    }
    if (path[0] != '/') {
        if (ffs_getcwd(joined, (unsigned) TB_FNAME_MAX) != 0) {
            return;
        }
        const char* colon = strchr(joined, ':');
        if (colon != NULL) {
            memmove(joined, colon + 1, strlen(colon + 1) + 1);
        }
        j = (int) strlen(joined);
        joined[j++] = '/';
    }
    const int plen = (int) strlen(path);
    if (j + plen >= (int) sizeof(joined)) {
        return;
    }
    memcpy(joined + j, path, (size_t) plen + 1);

    int n = 0;
    const char* p = joined;
    while (*p != 0) {
        while (*p == '/') {
            p++;
        }
        const char* e = p;
        while (*e != 0 && *e != '/') {
            e++;
        }
        const int len = (int) (e - p);
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            while (n > 0 && out[n - 1] != '/') {
                n--;
            }
            if (n > 0) {
                n--;
            }
        } else if (len > 0 && !(len == 1 && p[0] == '.')) {
            if (n + 1 + len >= max) {
                out[0] = 0;
                return;
            }
            out[n++] = '/';
            memcpy(out + n, p, (size_t) len);
            n += len;
        }
        p = e;
    }
    if (n == 0) {
        out[n++] = '/';
    }
    out[n] = 0;
}

// Whether two names are one file: resolved, then compared as FAT compares
// them, without regard to case.
static bool same_file(const char* a, const char* b) {
    static char ra[TB_FNAME_MAX];
    static char rb[TB_FNAME_MAX];
    resolve(ra, TB_FNAME_MAX, a);
    resolve(rb, TB_FNAME_MAX, b);
    if (ra[0] == 0 || rb[0] == 0) {
        return false;
    }
    for (int i = 0;; i++) {
        char x = ra[i];
        char y = rb[i];
        if (x >= 'a' && x <= 'z') {
            x = (char) (x - 'a' + 'A');
        }
        if (y >= 'a' && y <= 'z') {
            y = (char) (y - 'a' + 'A');
        }
        if (x != y) {
            return false;
        }
        if (x == 0) {
            return true;
        }
    }
}

static void show(editor* ed, int i) {
    current = i;
    ed_doc_show(ed, win_doc(ed, i), win_view(ed, i));
}

int win_find(editor* ed, const char* fname) {
    for (int i = 0; i < ADE_WINDOWS; i++) {
        document* d = win_doc(ed, i);
        if (d != NULL && same_file(tb_fname(&d->buf_), fname)) {
            return i;
        }
    }

    return -1;
}

void win_go(editor* ed, int i) {
    if (win_doc(ed, i) != NULL) {
        show(ed, i);
    }
}

int win_open_named(editor* ed, const char* fname) {
    int slot = -1;
    for (int i = 1; i < ADE_WINDOWS; i++) {
        if (!open_[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        ui_message(&ed->ui_, &ed->scr_,
                   "Three windows are open: close one to open another");
        return -1;
    }

    // One window per file. Two would each hold a copy, and whichever saved
    // last would write over the other's work; for a file too big for memory
    // the libraries refuse it anyway (TB_IN_USE), but a small one they allow.
    const int already = win_find(ed, fname);
    if (already >= 0) {
        static char msg[] = "Already open in window 1";
        msg[sizeof(msg) - 2] = (char) ('1' + already);
        ui_message(&ed->ui_, &ed->scr_, msg);
        return -1;
    }

    switch (ed_doc_open(ed, &docs[slot], &views[slot], ADE_DOC_KB, fname)) {
        case TB_OK:
            open_[slot] = true;
            current = slot;
            // ed_doc_open drew the header as it cleared the screen, before
            // the window was counted: the bar's list of windows did not have
            // it yet.
            scr_header_draw(&ed->scr_);

            return slot;
        case TB_TOO_LARGE:
            ui_message(&ed->ui_, &ed->scr_, "File too large, or no room for it");
            break;
        case TB_NO_FILE:
            ui_message(&ed->ui_, &ed->scr_, "Cannot open file");
            break;
        case TB_IN_USE:
            ui_message(&ed->ui_, &ed->scr_, "Already open in another window");
            break;
    }

    return -1;
}

void win_open(editor* ed) {
    // Said before asking for a name that could not be used anyway.
    if (win_doc(ed, 1) != NULL && win_doc(ed, 2) != NULL) {
        ui_message(&ed->ui_, &ed->scr_,
                   "Three windows are open: close one to open another");
        return;
    }

    char* typed;
    int sz;
    if (ui_text(&ed->ui_, &ed->scr_, "Open in a new window: ", NULL,
                &typed, &sz) != YES_OPT || sz <= 0) {
        return;
    }
    // What the prompt hands back is the text typed, without a terminator.
    static char fname[TB_FNAME_MAX];
    if (sz >= (int) sizeof(fname)) {
        ui_message(&ed->ui_, &ed->scr_, "Name too long");
        return;
    }
    memcpy(fname, typed, (size_t) sz);
    fname[sz] = 0;
    win_open_named(ed, fname);
}

void win_next(editor* ed) {
    for (int step = 1; step < ADE_WINDOWS; step++) {
        const int i = (current + step) % ADE_WINDOWS;
        if (open_[i]) {
            show(ed, i);
            return;
        }
    }
}

void win_close(editor* ed) {
    if (current == 0) {
        ui_message(&ed->ui_, &ed->scr_, "Window 1 stays open");
        return;
    }
    // The same bargain as opening a file over it: the work is about to go.
    if (tb_changed(&ed->doc_->buf_)) {
        const RESPONSE res = ui_dialog(&ed->ui_, &ed->scr_, "Save before closing?");
        if (res == CANCEL_OPT) {
            return;
        }
        if (res == YES_OPT && !cmd_save(ed)) {
            return;
        }
    }
    const int gone = current;
    int to = 0;
    for (int i = gone - 1; i >= 0; i--) {
        if (open_[i]) {
            to = i;
            break;
        }
    }
    show(ed, to);
    ed_doc_close(&docs[gone]);
    open_[gone] = false;

    // Showing the window it goes back to drew the header while this one was
    // still open, so the bar's list of windows still had it.
    scr_header_draw(&ed->scr_);
}

void win_list(editor* ed) {
    screen* scr = &ed->scr_;
    user_input* ui = &ed->ui_;
    const char top = scr->v_->topY_;
    const char bottom = ui->ypos_;
    const int width = scr->v_->cols_ < 255 ? scr->v_->cols_ : 255;
    static char line[256];

    int shown[ADE_WINDOWS];
    int n = 0;
    int at = 0;
    for (int i = 0; i < ADE_WINDOWS; i++) {
        if (open_[i]) {
            if (i == current) {
                at = n;
            }
            shown[n++] = i;
        }
    }

    for (;;) {
        char y = ui_modal_title(scr, top, width, "  WINDOWS");
        for (int k = 0; k < n && y < bottom; k++) {
            document* d = win_doc(ed, shown[k]);
            const char* name = tb_fname(&d->buf_);
            char num[2] = { (char) ('1' + shown[k]), 0 };
            int c = ui_put_at(line, 0, width, k == at ? "  > " : "    ");
            c = ui_put_at(line, c, width, num);
            c = ui_put_at(line, c, width, "  ");
            c = ui_put_at(line, c, width,
                          name != NULL && name[0] != 0 ? name : "(no name)");
            if (tb_changed(&d->buf_)) {
                c = ui_put_at(line, c, width, "  *");
            }
            scr_write_line(scr, y++, line, c);
        }
        ui_modal_fill(scr, y, bottom,
                      "  UP/DOWN to choose, RETURN to go there, ESC to stay", 52);

        const key_press kp = ks_wait(ui->keys_);
        const pick_key act = ui_modal_move(kp.vkey, &at, n - 1);
        if (act == PICK_LEAVE) {
            show(ed, current);          // put back what the list covered
            return;
        }
        if (act == PICK_TAKE) {
            show(ed, shown[at]);
            return;
        }
    }
}

void win_quit(editor* ed) {
    // Every window with unsaved work gets the question, shown as it is asked;
    // answering cancel to any of them calls the whole thing off.
    for (int i = 0; i < ADE_WINDOWS; i++) {
        document* d = win_doc(ed, i);
        if (d == NULL || !tb_changed(&d->buf_)) {
            continue;
        }
        show(ed, i);
        if (!cmd_quit(ed)) {
            return;
        }
    }
    ed->leaving_ = true;
}
