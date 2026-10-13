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

#include "newproject.h"

#include <agon/mos.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_ops.h"
#include "project.h"
#include "say.h"
#include "text.h"
#include "user_input.h"

// A program's name is its file, name.bin, and %n in the rules: one word, with
// nothing that would make those something else. The typed text into out;
// false, having said so, when it will not do.
static bool program_name(editor* ed, const char* typed, int sz, char* out) {
    bool bad = sz >= PROJ_NAME_MAX;
    for (int i = 0; i < sz; i++) {
        const char c = typed[i];
        if (c <= ' ' || c == '/' || c == '.' || c == ':') {
            bad = true;
        }
    }
    if (bad) {
        say(ed, "A program's name is one word, with no '/', '.' or ':'");
        return false;
    }
    memcpy(out, typed, (size_t) sz);
    out[sz] = 0;

    return true;
}

// Deletes `dir`/`name``ext`, if it is there.
static void delete_file(const char* dir, const char* name, const char* ext) {
    static char path[PROJ_NAME_MAX + 16];
    text t;
    text_start(&t, path, (int) sizeof(path));
    text_str(&t, dir);
    text_str(&t, "/");
    text_str(&t, name);
    text_str(&t, ext);
    mos_del(path);
}

// The project here, renamed.
static void rename_project(editor* ed) {
    static char msg[80];
    int has = 0;
    project* p = proj_read(&has, msg, (int) sizeof(msg));
    if (p == NULL || has < 0) {
        free(p);
        say(ed, msg);
        return;
    }
    static char old[PROJ_NAME_MAX];
    strcpy(old, p->name);
    free(p);

    char* typed = NULL;
    int sz = 0;
    if (ui_text(&ed->ui_, &ed->scr_, "Rename the project:", old, &typed, &sz) != YES_OPT
            || sz <= 0) {
        cmd_restore_after_modal(ed, false);
        return;
    }
    static char chosen[PROJ_NAME_MAX];
    if (!program_name(ed, typed, sz, chosen)) {
        return;
    }
    text t;
    text_start(&t, msg, (int) sizeof(msg));
    if (strcmp(chosen, old) == 0) {
        text_str(&t, "Project ");
        text_str(&t, old);
        text_str(&t, ": the name is as it was");
        say(ed, msg);
        return;
    }
    if (!proj_rename(chosen, msg, (int) sizeof(msg))) {
        say(ed, msg);
        return;
    }
    // Left, the old program would sit beside the new one as if it were still
    // the project's.
    delete_file(PROJ_BIN_DIR, old, ".bin");
    delete_file(PROJ_OBJ_DIR, old, PROJ_LIST_EXT);

    proj_forget();
    scr_header_draw(&ed->scr_);
    text_start(&t, msg, (int) sizeof(msg));
    text_str(&t, "Project ");
    text_str(&t, chosen);
    text_str(&t, ", was ");
    text_strn(&t, old, 24);
    text_str(&t, ": its old program is gone");
    say(ed, msg);
}

// The file on screen's name without its directory or extension: what a new
// project is usually named for, the file holding main.
static void name_offered(editor* ed, char* out, int max) {
    out[0] = 0;
    const char* name = tb_fname(&ed->doc_->buf_);
    if (name == NULL) {
        return;
    }
    const char* slash = strrchr(name, '/');
    const char* from = slash != NULL ? slash + 1 : name;
    const char* dot = strrchr(from, '.');
    const int len = dot != NULL ? (int) (dot - from) : (int) strlen(from);
    if (len < max) {
        memcpy(out, from, (size_t) len);
        out[len] = 0;
    }
}

void new_project(editor* ed) {
    const char here = mos_fopen(PROJ_FILE, FA_READ);
    if (here != 0) {
        mos_fclose(here);
        rename_project(ed);
        return;
    }

    static char offer[PROJ_NAME_MAX];
    name_offered(ed, offer, (int) sizeof(offer));
    char* typed = NULL;
    int sz = 0;
    if (ui_text(&ed->ui_, &ed->scr_, "Name the program:", offer, &typed, &sz) != YES_OPT
            || sz <= 0) {
        cmd_restore_after_modal(ed, false);
        return;
    }
    static char chosen[PROJ_NAME_MAX];
    if (!program_name(ed, typed, sz, chosen)) {
        return;
    }
    static char msg[80];
    if (!proj_create(chosen, msg, (int) sizeof(msg))) {
        say(ed, msg);
        return;
    }

    proj_forget();                      // the header names it from now on
    scr_header_draw(&ed->scr_);
    text t;
    text_start(&t, msg, (int) sizeof(msg));
    text_str(&t, "Project ");
    text_str(&t, chosen);
    text_str(&t, ": CTRL+B builds every .c, .s and .asm here");
    say(ed, msg);
}
