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
//
// A window is a file and where its cursor is; a document is that file loaded
// for editing, and only ADE_LOADED of them fit. So the windows -- up to
// ADE_WINDOWS -- are kept apart from the slots documents load into. Slot 0 is
// the editor's own document, shown in the whole screen's view; the others are
// ade's, each with a view of its own. A window shown when it is not loaded
// takes a free slot, or the slot of the loaded window used longest ago that
// has its work saved -- which is put away, keeping only its name and cursor
// -- or, when every loaded window has unsaved work, asks to save one.
typedef struct {
    char name[ADE_NAME_MAX];            // the file, kept while put away
    signed char slot;                   // the slot it is loaded in, or -1
    int line;                           // its cursor, kept while put away
    int x;
    unsigned long used;                 // when it was last on screen
} window;

static window wins[ADE_WINDOWS];
static int nwin;
static int current;                     // the window on screen
static unsigned long clock_;            // counts showings, for "longest ago"

static document docs[ADE_LOADED];       // [0] unused: slot 0 is the editor's
static view views[ADE_LOADED];          // [0] unused: slot 0 has whole_
static bool used[ADE_LOADED];           // which slots hold a document

static document* slot_doc(editor* ed, int s) {
    return s == 0 ? &ed->home_ : &docs[s];
}

static view* slot_view(editor* ed, int s) {
    return s == 0 ? &ed->scr_.whole_ : &views[s];
}

document* win_doc(editor* ed, int i) {
    if (i < 0 || i >= nwin || wins[i].slot < 0) {
        return NULL;
    }

    return slot_doc(ed, wins[i].slot);
}

const char* win_name(editor* ed, int i) {
    if (i < 0 || i >= nwin) {
        return NULL;
    }
    document* d = win_doc(ed, i);

    return d != NULL ? tb_fname(&d->buf_) : wins[i].name;
}

bool win_loaded(int i) {
    return i >= 0 && i < nwin && wins[i].slot >= 0;
}

int win_count(void) {
    return nwin;
}

void win_where(editor* ed, int i, int* line, int* x) {
    document* d = win_doc(ed, i);
    if (d != NULL) {
        *line = tb_ypos(&d->buf_);
        *x = tb_xpos(&d->buf_) - 1;
    } else {
        *line = i >= 0 && i < nwin ? wins[i].line : 1;
        *x = i >= 0 && i < nwin ? wins[i].x : 0;
    }
}

void win_place(editor* ed, int i, int line, int x) {
    if (i < 0 || i >= nwin) {
        return;
    }
    if (wins[i].slot >= 0) {
        ed_doc_place(ed, slot_doc(ed, wins[i].slot), slot_view(ed, wins[i].slot),
                     line, x);
    } else {
        wins[i].line = line;
        wins[i].x = x;
    }
}

int win_strip(char* out, int max) {
    int n = 0;
    for (int i = 0; i < nwin && n + 6 < max; i++) {
        if (n > 0) {
            out[n++] = ' ';
        }
        if (i == current) {
            out[n++] = '[';
        }
        if (i + 1 >= 10) {
            out[n++] = (char) ('0' + (i + 1) / 10);
        }
        out[n++] = (char) ('0' + (i + 1) % 10);
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
    memset(used, 0, sizeof(used));
    used[0] = true;
    memset(wins, 0, sizeof(wins));
    wins[0].slot = 0;
    const char* name = tb_fname(&ed->home_.buf_);
    if (name != NULL) {
        strncpy(wins[0].name, name, ADE_NAME_MAX - 1);
    }
    wins[0].used = clock_ = 1;
    nwin = 1;
    current = 0;
}

void win_finish(editor* ed) {
    // ed_destroy takes the editor's own document with it, so it has to be
    // open to be taken: if its slot was given up, it opens again, empty.
    if (!used[0]) {
        ed_doc_open(ed, &ed->home_, &ed->scr_.whole_, ADE_DOC_KB, NULL);
        used[0] = true;
    }
    ed_doc_show(ed, &ed->home_, &ed->scr_.whole_);
    for (int s = 1; s < ADE_LOADED; s++) {
        if (used[s]) {
            ed_doc_close(&docs[s]);
            used[s] = false;
        }
    }
    nwin = 0;
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

// Window i's document put away: its name and cursor kept, the slot freed.
static void put_away(editor* ed, int i) {
    document* d = slot_doc(ed, wins[i].slot);
    const char* name = tb_fname(&d->buf_);
    strncpy(wins[i].name, name != NULL ? name : "", ADE_NAME_MAX - 1);
    wins[i].name[ADE_NAME_MAX - 1] = 0;
    wins[i].line = tb_ypos(&d->buf_);
    wins[i].x = tb_xpos(&d->buf_) - 1;
    ed_doc_close(d);
    used[wins[i].slot] = false;
    wins[i].slot = -1;
}

// Whether a loaded window can be put away as it is: its work saved, and a
// name short enough to keep, to load it from again.
static bool can_put_away(editor* ed, int i) {
    document* d = slot_doc(ed, wins[i].slot);
    const char* name = tb_fname(&d->buf_);

    return !tb_changed(&d->buf_) && name != NULL && name[0] != 0
           && strlen(name) < ADE_NAME_MAX;
}

// A slot for a document, other than window `keep`'s: a free one, or one a
// window gives up -- the one used longest ago with its work saved, or, when
// none has, the one used longest ago once it is saved, asked first. -1 when
// the reader would rather not, or it could not be saved.
static int free_slot(editor* ed, int keep) {
    for (int s = 0; s < ADE_LOADED; s++) {
        if (!used[s]) {
            return s;
        }
    }
    int clean = -1;
    int any = -1;
    for (int i = 0; i < nwin; i++) {
        if (i == keep || wins[i].slot < 0) {
            continue;
        }
        if (any < 0 || wins[i].used < wins[any].used) {
            any = i;
        }
        if (can_put_away(ed, i) && (clean < 0 || wins[i].used < wins[clean].used)) {
            clean = i;
        }
    }
    if (clean < 0 && any >= 0) {
        document* d = slot_doc(ed, wins[any].slot);
        const char* name = tb_fname(&d->buf_);
        if (name == NULL || name[0] == 0) {
            ui_message(&ed->ui_, &ed->scr_,
                       "Three files are open with no name: save one first");
            return -1;
        }
        static char ask[ADE_NAME_MAX + 40];
        strcpy(ask, "Save ");
        strncat(ask, name, ADE_NAME_MAX - 1);
        strcat(ask, " to make room?");
        if (ui_dialog(&ed->ui_, &ed->scr_, ask) != YES_OPT) {
            return -1;
        }
        if (!tb_save(&d->buf_)) {
            ui_message(&ed->ui_, &ed->scr_, "It could not be saved");
            return -1;
        }
        clean = any;
    }
    if (clean < 0) {
        return -1;
    }
    const int s = wins[clean].slot;
    put_away(ed, clean);

    return s;
}

// Window i, loaded if it was put away and back where its cursor was; false,
// having said why, when it could not be.
static bool load(editor* ed, int i) {
    if (wins[i].slot >= 0) {
        return true;
    }
    const int s = free_slot(ed, i);
    if (s < 0) {
        return false;
    }
    if (ed_doc_open(ed, slot_doc(ed, s), slot_view(ed, s), ADE_DOC_KB,
                    wins[i].name) != TB_OK) {
        ui_message(&ed->ui_, &ed->scr_, "Cannot open its file any more");
        return false;
    }
    used[s] = true;
    wins[i].slot = (signed char) s;
    ed_doc_place(ed, slot_doc(ed, s), slot_view(ed, s), wins[i].line, wins[i].x);

    return true;
}

static void show(editor* ed, int i) {
    if (!load(ed, i)) {
        cmd_restore_after_modal(ed, false);
        return;
    }
    current = i;
    wins[i].used = ++clock_;
    ed_doc_show(ed, slot_doc(ed, wins[i].slot), slot_view(ed, wins[i].slot));
}

int win_find(editor* ed, const char* fname) {
    for (int i = 0; i < nwin; i++) {
        if (same_file(win_name(ed, i), fname)) {
            return i;
        }
    }

    return -1;
}

void win_go(editor* ed, int i) {
    if (i >= 0 && i < nwin) {
        show(ed, i);
    }
}

int win_restore(editor* ed, const char* fname, int line, int x) {
    (void) ed;
    if (nwin == ADE_WINDOWS || fname == NULL || strlen(fname) >= ADE_NAME_MAX) {
        return -1;
    }
    window* w = &wins[nwin];
    memset(w, 0, sizeof(*w));
    strcpy(w->name, fname);
    w->slot = -1;
    w->line = line;
    w->x = x;

    return nwin++;
}

int win_open_named(editor* ed, const char* fname) {
    if (nwin == ADE_WINDOWS) {
        ui_message(&ed->ui_, &ed->scr_, "Ten windows are open: close one to open another");
        return -1;
    }

    // One window per file. Two would each hold a copy, and whichever saved
    // last would write over the other's work; for a file too big for memory
    // the libraries refuse it anyway (TB_IN_USE), but a small one they allow.
    const int already = win_find(ed, fname);
    if (already >= 0) {
        static char msg[] = "Already open in window 10";
        const int n = already + 1;
        int k = (int) sizeof(msg) - 3;
        if (n >= 10) {
            msg[k++] = (char) ('0' + n / 10);
        }
        msg[k++] = (char) ('0' + n % 10);
        msg[k] = 0;
        ui_message(&ed->ui_, &ed->scr_, msg);
        return -1;
    }
    if (strlen(fname) >= ADE_NAME_MAX) {
        ui_message(&ed->ui_, &ed->scr_, "Name too long");
        return -1;
    }

    const int slot = free_slot(ed, -1);
    if (slot < 0) {
        cmd_restore_after_modal(ed, false);
        return -1;
    }
    // The last window, in whichever slot was free.
    switch (ed_doc_open(ed, slot_doc(ed, slot), slot_view(ed, slot), ADE_DOC_KB,
                        fname)) {
        case TB_OK: {
            used[slot] = true;
            window* w = &wins[nwin];
            memset(w, 0, sizeof(*w));
            strcpy(w->name, fname);
            w->slot = (signed char) slot;
            w->used = ++clock_;
            current = nwin++;
            // ed_doc_open drew the header as it cleared the screen, before
            // the window was counted: the bar's list of windows did not have
            // it yet.
            scr_header_draw(&ed->scr_);

            return current;
        }
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
    // Back to the window that was on screen; a window put away to make room
    // is still put away.
    show(ed, current);

    return -1;
}

void win_open(editor* ed) {
    // Said before asking for a name that could not be used anyway.
    if (nwin == ADE_WINDOWS) {
        ui_message(&ed->ui_, &ed->scr_, "Ten windows are open: close one to open another");
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
    if (nwin > 1) {
        show(ed, (current + 1) % nwin);
    }
}

void win_prev(editor* ed) {
    if (nwin > 1) {
        show(ed, (current + nwin - 1) % nwin);
    }
}

// To window n, from 0, if there is one and it is not on screen; else nothing.
static void to(editor* ed, int n) {
    if (n < nwin && n != current) {
        show(ed, n);
    }
}

void win_to_1(editor* ed) { to(ed, 0); }
void win_to_2(editor* ed) { to(ed, 1); }
void win_to_3(editor* ed) { to(ed, 2); }
void win_to_4(editor* ed) { to(ed, 3); }
void win_to_5(editor* ed) { to(ed, 4); }
void win_to_6(editor* ed) { to(ed, 5); }
void win_to_7(editor* ed) { to(ed, 6); }
void win_to_8(editor* ed) { to(ed, 7); }
void win_to_9(editor* ed) { to(ed, 8); }
void win_to_10(editor* ed) { to(ed, 9); }

void win_close(editor* ed) {
    if (nwin == 1) {
        ui_message(&ed->ui_, &ed->scr_, "The last window stays open");
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
    // To the window before it -- or, closing window 1, the one after, which
    // is window 1 then. Shown first: a document cannot be closed while it is
    // the one on screen. That may load it, putting another away -- never
    // this one, which is on screen and so the last to go.
    const int gone = current;
    const int to = gone > 0 ? gone - 1 : 1;
    show(ed, to);
    if (current != to) {
        return;                         // it could not be shown: stay as we are
    }
    if (wins[gone].slot >= 0) {
        ed_doc_close(slot_doc(ed, wins[gone].slot));
        used[wins[gone].slot] = false;
    }

    // The ones after it move down a number.
    for (int i = gone; i + 1 < nwin; i++) {
        wins[i] = wins[i + 1];
    }
    nwin--;
    current = to > gone ? to - 1 : to;

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
    int at = current;

    for (;;) {
        char y = ui_modal_title(scr, top, width, "  WINDOWS");
        for (int k = 0; k < nwin && y < bottom; k++) {
            const char* name = win_name(ed, k);
            char num[3] = { 0, 0, 0 };
            if (k + 1 >= 10) {
                num[0] = (char) ('0' + (k + 1) / 10);
                num[1] = (char) ('0' + (k + 1) % 10);
            } else {
                num[0] = (char) ('1' + k);
            }
            int c = ui_put_at(line, 0, width, k == at ? "  > " : "    ");
            c = ui_put_at(line, c, width, num);
            c = ui_put_at(line, c, width, k + 1 >= 10 ? " " : "  ");
            c = ui_put_at(line, c, width,
                          name != NULL && name[0] != 0 ? name : "(no name)");
            document* d = win_doc(ed, k);
            if (d != NULL && tb_changed(&d->buf_)) {
                c = ui_put_at(line, c, width, "  *");
            }
            scr_write_line(scr, y++, line, c);
        }
        ui_modal_fill(scr, y, bottom,
                      "  UP/DOWN to choose, RETURN to go there, ESC to stay", 52);

        const key_press kp = ks_wait(ui->keys_);
        const pick_key act = ui_modal_move(kp.vkey, &at, nwin - 1);
        if (act == PICK_LEAVE) {
            show(ed, current);          // put back what the list covered
            return;
        }
        if (act == PICK_TAKE) {
            show(ed, at);
            return;
        }
    }
}

void win_quit(editor* ed) {
    // Every window with unsaved work gets the question, shown as it is asked;
    // answering cancel to any of them calls the whole thing off. A window
    // put away has none: it was saved to be put away.
    for (int i = 0; i < nwin; i++) {
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
