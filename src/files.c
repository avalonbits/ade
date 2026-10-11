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

#include "files.h"

#include <agon/mos.h>
#include <stdlib.h>
#include <string.h>

#include "text_buffer.h"
#include "user_input.h"
#include "windows.h"

// What one directory can list: entries past either are left out, and the
// list says so.
#define LIST_MAX 128
#define POOL_MAX 3072

// The directory being shown, read whole when the viewer goes into it and
// given back when the viewer closes: some 4 KB, held only while it is up.
typedef struct {
    int n;
    int used;
    bool more;                          // entries left out for want of room
    int off[LIST_MAX];                  // where each name starts in pool
    bool dir[LIST_MAX];
    unsigned char shown[LIST_MAX];      // those the typed text matches
    int nshown;
    char pool[POOL_MAX];

    // Where the viewer is, under the directory ade started in: "" there,
    // else "src" or "src/lib". And what has been typed.
    char sub[TB_FNAME_MAX];
    char typed[TB_FNAME_MAX];
    int ntyped;

    // Room to work in -- names joined, resolved, typed -- and a row: here
    // rather than static, so that none of it is held while the viewer is
    // closed.
    char a[TB_FNAME_MAX];
    char b[TB_FNAME_MAX];
    char line[256];
    DIR dirh;
    FILINFO info;
} listing;

static listing* list;

#define sub    (list->sub)
#define typed  (list->typed)
#define ntyped (list->ntyped)

// Rows: [ New directory ], then ".." below the top, then the names shown.
#define ROW_NEWDIR 0

static int up_rows(void) {
    return sub[0] != 0 ? 1 : 0;
}

static int first_name_row(void) {
    return 1 + up_rows();
}

static int rows(void) {
    return first_name_row() + list->nshown;
}

static const char* name_at(int i) {
    return list->pool + list->off[i];
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char) (c - 'A' + 'a') : c;
}

// As FAT orders nothing, by name without regard to case.
static int compare(const char* a, const char* b) {
    for (;; a++, b++) {
        const char x = lower(*a);
        const char y = lower(*b);
        if (x != y || x == 0) {
            return (unsigned char) x - (unsigned char) y;
        }
    }
}

// Whether `name` holds `part`, without regard to case.
static bool holds(const char* name, const char* part, int n) {
    for (const char* s = name; *s != 0; s++) {
        int i = 0;
        while (i < n && s[i] != 0 && lower(s[i]) == lower(part[i])) {
            i++;
        }
        if (i == n) {
            return true;
        }
    }

    return false;
}

static void narrow(void) {
    list->nshown = 0;
    for (int i = 0; i < list->n; i++) {
        if (ntyped == 0 || holds(name_at(i), typed, ntyped)) {
            list->shown[list->nshown++] = (unsigned char) i;
        }
    }
}

// Reads the directory the viewer is in: directories first, then files, each
// by name. Names starting with "." are left out.
static void read_list(void) {
    DIR* dir = &list->dirh;
    FILINFO* info = &list->info;
    list->n = 0;
    list->used = 0;
    list->more = false;
    if (ffs_dopen(dir, sub[0] != 0 ? sub : ".") == 0) {
        while (ffs_dread(dir, info) == 0 && info->fname[0] != 0) {
            if (info->fname[0] == '.') {
                continue;
            }
            const int len = (int) strlen(info->fname);
            if (list->n == LIST_MAX || list->used + len + 1 > POOL_MAX) {
                list->more = true;
                continue;
            }
            const bool isdir = (info->fattrib & AM_DIR) != 0;
            const int off = list->used;
            memcpy(list->pool + off, info->fname, (size_t) len + 1);
            list->used += len + 1;

            // Into its place, the ones after it moved along one.
            int at = list->n++;
            while (at > 0) {
                const bool pdir = list->dir[at - 1];
                const bool after = pdir == isdir
                                   ? compare(name_at(at - 1), info->fname) > 0
                                   : isdir;
                if (!after) {
                    break;
                }
                list->off[at] = list->off[at - 1];
                list->dir[at] = pdir;
                at--;
            }
            list->off[at] = off;
            list->dir[at] = isdir;
        }
        ffs_dclose(dir);
    }
    ntyped = 0;
    typed[0] = 0;
    narrow();
}

// `name` under the directory the viewer is in, into out. False when it does
// not fit.
static bool join(char* out, int max, const char* name) {
    const int s = (int) strlen(sub);
    const int n = (int) strlen(name);
    if (s + 1 + n >= max) {
        return false;
    }
    int at = 0;
    if (s > 0 && name[0] != '/') {
        memcpy(out, sub, (size_t) s);
        out[s] = '/';
        at = s + 1;
    }
    memcpy(out + at, name, (size_t) n + 1);

    return true;
}

// Whether `path` is the directory ade started in or under it.
static bool inside(const char* path) {
    char* root = list->a;
    char* full = list->b;
    win_resolve(root, TB_FNAME_MAX, ".");
    win_resolve(full, TB_FNAME_MAX, path);
    if (root[0] == 0 || full[0] == 0) {
        return false;
    }
    if (root[1] == 0) {
        return true;                    // the card's root holds everything
    }
    const int n = (int) strlen(root);
    for (int i = 0; i < n; i++) {
        if (lower(root[i]) != lower(full[i])) {
            return false;
        }
    }

    return full[n] == 0 || full[n] == '/';
}

static void draw(editor* ed, const char* title, int at, int* first) {
    screen* scr = &ed->scr_;
    const char top = scr->v_->topY_;
    const char bottom = ed->ui_.ypos_;
    const int width = scr->v_->cols_ < 255 ? scr->v_->cols_ : 255;
    char* line = list->line;

    char y = ui_modal_title(scr, top, width, title);
    int c = ui_put_at(line, 0, width, "  In /");
    c = ui_put_at(line, c, width, sub);
    if (list->more) {
        c = ui_put_at(line, c, width, "   (not all of it: too many names)");
    }
    scr_write_line(scr, y++, line, c);

    // The rows between, the name bar under them, the keys under that.
    const int room = bottom - 1 - y;
    if (at < *first) {
        *first = at;
    }
    if (room > 0 && at >= *first + room) {
        *first = at - room + 1;
    }
    for (int r = *first; r < rows() && y < bottom - 1; r++) {
        c = ui_put_at(line, 0, width, r == at ? "  > " : "    ");
        if (r == ROW_NEWDIR) {
            c = ui_put_at(line, c, width, "[ New directory ]");
        } else if (r < first_name_row()) {
            c = ui_put_at(line, c, width, "../");
        } else {
            const int i = list->shown[r - first_name_row()];
            c = ui_put_at(line, c, width, name_at(i));
            if (list->dir[i]) {
                c = ui_put_at(line, c, width, "/");
            }
        }
        scr_write_line(scr, y++, line, c);
    }
    while (y < bottom - 1) {
        scr_write_line(scr, y++, NULL, 0);
    }
    c = ui_put_at(line, 0, width, "  Name: ");
    c = ui_put_at(line, c, width, typed);
    c = ui_put_at(line, c, width, "_");
    scr_write_line(scr, y, line, c);
    static const char keys[] =
        "  Type to narrow, RETURN to open, F7 a new directory, ESC to leave";
    ui_modal_fill(scr, bottom, bottom, keys, (int) sizeof(keys) - 1);
}

// Asks for a directory's name and makes it where the viewer is. The row it
// lands on, or -1.
static int new_dir(editor* ed) {
    char* name;
    int sz;
    if (ui_text(&ed->ui_, &ed->scr_, "New directory: ", NULL, &name, &sz) != YES_OPT
            || sz <= 0) {
        return -1;
    }
    char* base = list->a;
    char* path = list->b;
    if (sz >= TB_FNAME_MAX) {
        ui_message(&ed->ui_, &ed->scr_, "Name too long");
        return -1;
    }
    memcpy(base, name, (size_t) sz);
    base[sz] = 0;
    if (strchr(base, '/') != NULL || strchr(base, ':') != NULL
            || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        ui_message(&ed->ui_, &ed->scr_, "A directory's name, here: no / or ..");
        return -1;
    }
    if (!join(path, TB_FNAME_MAX, base) || mos_mkdir(path) != 0) {
        ui_message(&ed->ui_, &ed->scr_, "The directory could not be made");
        return -1;
    }
    read_list();
    for (int k = 0; k < list->nshown; k++) {
        if (list->dir[list->shown[k]] && compare(name_at(list->shown[k]), base) == 0) {
            return first_name_row() + k;
        }
    }

    return -1;
}

// What RETURN does on row `at`: true with a name in out, to open.
static bool take(editor* ed, int* at, char* out, int max) {
    // Typed, and nothing listed holds it: a new file of that name.
    if (ntyped > 0 && list->nshown == 0) {
        if (!join(out, max, typed)) {
            ui_message(&ed->ui_, &ed->scr_, "Name too long");
            return false;
        }
        if (!inside(out)) {
            ui_message(&ed->ui_, &ed->scr_, "Outside the project: not opened");
            return false;
        }

        return true;
    }
    if (*at == ROW_NEWDIR) {
        const int r = new_dir(ed);
        *at = r >= 0 ? r : ROW_NEWDIR;
        return false;
    }
    if (*at < first_name_row()) {
        char* slash = strrchr(sub, '/');
        if (slash != NULL) {
            *slash = 0;
        } else {
            sub[0] = 0;
        }
        read_list();
        *at = first_name_row() < rows() ? first_name_row() : ROW_NEWDIR;
        return false;
    }
    const int i = list->shown[*at - first_name_row()];
    if (list->dir[i]) {
        char* into = list->a;
        if (!join(into, TB_FNAME_MAX, name_at(i))) {
            ui_message(&ed->ui_, &ed->scr_, "Name too long");
            return false;
        }
        strcpy(sub, into);
        read_list();
        *at = first_name_row() < rows() ? first_name_row() : ROW_NEWDIR;
        return false;
    }
    if (!join(out, max, name_at(i))) {
        ui_message(&ed->ui_, &ed->scr_, "Name too long");
        return false;
    }

    return true;
}

bool files_pick(editor* ed, const char* title, char* out, int max) {
    list = malloc(sizeof(listing));
    if (list == NULL) {
        ui_message(&ed->ui_, &ed->scr_, "No room to list the files");
        return false;
    }
    sub[0] = 0;
    ntyped = 0;
    typed[0] = 0;
    read_list();
    int at = first_name_row() < rows() ? first_name_row() : ROW_NEWDIR;
    int first = 0;
    bool chosen = false;
    for (;;) {
        draw(ed, title, at, &first);
        const key_press kp = ks_wait(ed->ui_.keys_);
        if (kp.vkey == VK_F7) {
            const int r = new_dir(ed);
            at = r >= 0 ? r : at;
            continue;
        }
        if (kp.vkey == VK_BACKSPACE) {
            if (ntyped > 0) {
                typed[--ntyped] = 0;
                narrow();
                at = list->nshown > 0 ? first_name_row() : ROW_NEWDIR;
            }
            continue;
        }
        const bool plain = (kp.mods & (MOD_CTRL | MOD_ALT)) == 0;
        if (plain && kp.ch >= ' ' && kp.ch < 127 && ntyped < TB_FNAME_MAX - 1) {
            typed[ntyped++] = kp.ch;
            typed[ntyped] = 0;
            narrow();
            at = list->nshown > 0 ? first_name_row() : ROW_NEWDIR;
            continue;
        }
        const pick_key act = ui_modal_move(kp.vkey, &at, rows() - 1);
        if (act == PICK_LEAVE) {
            break;
        }
        if (act == PICK_TAKE && take(ed, &at, out, max)) {
            chosen = true;
            break;
        }
    }
    free(list);
    list = NULL;

    return chosen;
}
