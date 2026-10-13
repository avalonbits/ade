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

#include "away.h"

#include <agon/mos.h>
#include <string.h>

#include <hub/hub.h>

#include "build.h"
#include "lookup.h"
#include "mode.h"
#include "say.h"
#include "shell.h"
#include "text.h"
#include "windows.h"

// The session's block in hub's memory. hub keeps it while ade is gone.
static session* block(void) {
    return (session*) hub_block(SESSION_TAG, sizeof(session));
}

// Whether `s` holds a session this ade wrote and has not come back to yet.
// The size is checked as well as MAGIC: hub keeps a block between runs, so an
// older ade's session, laid out differently, can be found in it.
static bool kept(const session* s) {
    return s != NULL && memcmp(s->magic, MAGIC, 4) == 0 && s->size == sizeof(session);
}

bool away_save_windows(editor* ed) {
    static char msg[48];
    for (int i = 0; i < win_count(); i++) {
        // A window put away has nothing to save: it was saved to be put away.
        document* d = win_doc(ed, i);
        if (d == NULL || !tb_changed(&d->buf_)) {
            continue;
        }
        const char* name = tb_fname(&d->buf_);
        const bool named = name != NULL && name[0] != 0;
        if (named && tb_save(&d->buf_)) {
            continue;
        }
        text t;
        text_start(&t, msg, (int) sizeof(msg));
        text_str(&t, "Window ");
        text_int(&t, i + 1);
        text_str(&t, named ? " could not be saved" : " has no name: save it first");
        say(ed, msg);
        return false;
    }

    return true;
}

session* away_keep(editor* ed, unsigned char kind) {
    session* s = block();
    if (s == NULL) {
        say(ed, "hub has no room to keep ade's windows");
        return NULL;
    }
    memset(s, 0, sizeof(*s));
    memcpy(s->magic, MAGIC, 4);
    s->size = sizeof(session);
    s->kind = kind;
    s->mode = (unsigned char) mode_now();
    s->current = (unsigned char) win_current();
    for (int i = 0; i < ADE_WINDOWS; i++) {
        const char* name = win_name(ed, i);
        s->open[i] = name != NULL && strlen(name) < TB_FNAME_MAX;
        if (s->open[i]) {
            strcpy(s->name[i], name);
            win_where(ed, i, &s->line[i], &s->x[i]);
        }
    }

    return s;
}

bool away_hand_over(editor* ed, session* s, const char* job, unsigned char flags,
                    const char* then, unsigned char then_flags, const char* what) {
    bool queued = hub_enter(SESSION_TAG) == HUB_OK && hub_push(job, flags) == HUB_OK;
    if (queued && then != NULL) {
        queued = hub_push(then, then_flags) == HUB_OK;
    }
    if (!queued || hub_return_to("ade -resume") != HUB_OK) {
        memset(s->magic, 0, 4);         // nothing to come back to
        static char msg[48];
        text t;
        text_start(&t, msg, (int) sizeof(msg));
        text_str(&t, "hub would not ");
        text_str(&t, what);
        say(ed, msg);
        return false;
    }
    ed->leaving_ = true;

    return true;
}

void away_screen_back(void) {
    const session* s = block();
    if (kept(s) && s->mode != mode_now()) {
        mode_set(s->mode);
    }
}

const char* away_first_file(void) {
    const session* s = block();
    if (!kept(s) || !s->open[0] || s->name[0][0] == 0) {
        return NULL;
    }

    return s->name[0];
}

void away_come_back(editor* ed) {
    session* s = block();
    if (!kept(s)) {
        return;
    }
    memset(s->magic, 0, 4);             // used: a later start is not a resume

    // Window 1 is open already: the editor started on its file. The others
    // come back put away -- each is loaded when it is shown -- unless its name
    // is too long to keep put away, when it is loaded now.
    for (int i = 1; i < ADE_WINDOWS; i++) {
        if (s->open[i] && win_restore(ed, s->name[i], s->line[i], s->x[i]) < 0) {
            const int w = win_open_named(ed, s->name[i]);
            if (w >= 0) {
                win_place(ed, w, s->line[i], s->x[i]);
            }
        }
    }

    // Where ade lands: the window that was on screen -- or, after a failed
    // build, its first error.
    int target = s->open[s->current] ? win_find(ed, s->name[s->current]) : -1;
    if (target < 0) {
        target = 0;
    }
    int line = s->line[s->current];
    int x = s->x[s->current];
    if (s->kind == SESSION_BUILD) {
        build_landing(ed, s, &target, &line, &x);
    }

    // Every other window placed without being drawn, then the landing one
    // shown: a paint costs a fifth of a second, so there is only one.
    for (int i = 0; i < ADE_WINDOWS; i++) {
        const int w = s->open[i] ? win_find(ed, s->name[i]) : -1;
        if (w >= 0 && w != target) {
            win_place(ed, w, s->line[i], s->x[i]);
        }
    }
    win_show_at(ed, target, line, x);

    // And what the job said, if anything.
    switch (s->kind) {
        case SESSION_BUILD:
            build_report(ed, s);
            break;
        case SESSION_SHELL:
            shell_report(ed);
            break;
        case SESSION_REF:
            lookup_report(ed, s);
            break;
    }
}
