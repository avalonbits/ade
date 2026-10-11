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

#include "menu.h"

#include <agon/mos.h>
#include <string.h>

#include "build.h"
#include "cmd_ops.h"
#include "options.h"
#include "project.h"
#include "text.h"
#include "user_input.h"
#include "windows.h"

// One entry: what it says, the key that does the same, and the command with
// the flags that key carries. `later` names a menu entry whose command has not
// arrived yet, and is what choosing it says.
typedef struct {
    const char* label;
    const char* keys;
    cmd_op cmd;
    char flags;
    const char* later;
} menu_item;

typedef struct {
    const char* name;
    const menu_item* items;
    int n;
} menu;

#define OWN KC_OWNS_SEL

static const menu_item FILE_ITEMS[] = {
    { "Open in new window", "CTRL+SHIFT+O", win_open,   0, NULL },
    { "Open here",          "CTRL+O",     cmd_open,     0, NULL },
    { "Save",               "CTRL+S",     ed_cmd_save,  0, NULL },
    { "Save as",            "CTRL+ALT+S", cmd_save_as,  0, NULL },
    { "Close window",       "ALT+F3",     win_close,    0, NULL },
    { "Next window",        "CTRL+TAB",   win_next,     0, NULL },
    { "Previous window",    "CTRL+SHIFT+TAB", win_prev, 0, NULL },
    { "Window list",        "ALT+F6",     win_list,     0, NULL },
    { "Shell",              "CTRL+F12",   build_shell,  0, NULL },
    { "Quit",               "CTRL+Q",     win_quit,     0, NULL },
};
static const menu_item EDIT_ITEMS[] = {
    { "Undo",               "CTRL+Z",     cmd_undo,       0,   NULL },
    { "Redo",               "CTRL+Y",     cmd_redo,       0,   NULL },
    { "Cut",                "CTRL+X",     cmd_cut,        OWN, NULL },
    { "Copy",               "CTRL+C",     cmd_copy,       OWN, NULL },
    { "Paste",              "CTRL+V",     cmd_paste,      OWN, NULL },
    { "Select all",         "CTRL+A",     cmd_select_all, OWN, NULL },
    { "Delete line",        "CTRL+D",     cmd_del_line,   0,   NULL },
};
static const menu_item SEARCH_ITEMS[] = {
    { "Find",               "CTRL+F",     cmd_find,       OWN, NULL },
    { "Find next",          "CTRL+N",     cmd_find_next,  OWN, NULL },
    { "Find previous",      "CTRL+P",     cmd_find_prev,  OWN, NULL },
    { "Go to line",         "CTRL+G",     cmd_goto,       0,   NULL },
};
static const menu_item BUILD_ITEMS[] = {
    { "Build",              "CTRL+B",     build, 0, NULL },
    { "Errors",             "ALT+F9",     build_errors, 0, NULL },
    { "New project",        "",           build_new_project, 0, NULL },
};
static const menu_item RUN_ITEMS[] = {
    { "Run",                "CTRL+R",     build_run, 0, NULL },
    { "User screen",        "ALT+F5",     build_user_screen, 0, NULL },
};
static const menu_item OPTIONS_ITEMS[] = {
    { "Settings",           "CTRL+E",     ade_cmd_settings, 0, NULL },
};

#undef OWN

#define ITEMS(a) a, (int) (sizeof(a) / sizeof(a[0]))

static const menu MENUS[] = {
    { "File",    ITEMS(FILE_ITEMS) },
    { "Edit",    ITEMS(EDIT_ITEMS) },
    { "Search",  ITEMS(SEARCH_ITEMS) },
    { "Build",   ITEMS(BUILD_ITEMS) },
    { "Run",     ITEMS(RUN_ITEMS) },
    { "Options", ITEMS(OPTIONS_ITEMS) },
};
#define NMENUS ((int) (sizeof(MENUS) / sizeof(MENUS[0])))

// Which menu is pulled down, or -1 for none: what the header shows.
static int open_menu = -1;

// Where menu `m`'s name starts on the header row. Each name takes its length
// and two spaces, after a space of margin.
static int menu_col(int m) {
    int col = 1;
    for (int i = 0; i < m; i++) {
        col += (int) strlen(MENUS[i].name) + 2;
    }

    return col;
}

int menu_bar_text(screen* scr, char* bar, int max) {
    int n = 0;
    bar[n++] = ' ';
    for (int m = 0; m < NMENUS && n < max - 12; m++) {
        const int len = (int) strlen(MENUS[m].name);
        bar[n - 1] = m == open_menu ? '[' : ' ';
        memcpy(bar + n, MENUS[m].name, (size_t) len);
        n += len;
        bar[n++] = m == open_menu ? ']' : ' ';
        bar[n++] = ' ';
    }

    // On the right, what is being worked on: how the last build went, if
    // there has been one, the project's program, if there is a project.ini,
    // and the open windows, the one on screen in brackets.
    static char right[PROJ_NAME_MAX + 60];
    const char* how = build_status();
    const char* name = build_project();
    text t;
    text_start(&t, right, (int) sizeof(right));
    if (how[0] != 0) {
        text_str(&t, how);
        text_str(&t, "  ");
    }
    if (name != NULL) {
        text_str(&t, name);
        text_str(&t, "  ");
    }
    int r = text_len(&t);
    r += win_strip(right + r, (int) sizeof(right) - r);
    const int width = scr->barW_ < max ? scr->barW_ : max;
    const int at = width - r - 1;
    if (at > n) {
        memset(bar + n, ' ', (size_t) (at - n));
        memcpy(bar + at, right, (size_t) r);
        n = at + r;
    }

    return n;
}

// The row under the title: the menus' names, the open one in brackets, and
// on the right the project and the windows.
//
// In the footer's colours -- the document's, the other way round -- across
// the whole row, as the footer does it: a viewport over the row cleared in
// them colours every column, the last included, which nothing may be printed
// in. The library puts the document's colours back after.
static void draw_bar(screen* scr, void* ctx) {
    (void) ctx;
    static char bar[128];
    const char paint[] = {
        17, scr->bg_, 17, (char) (scr->fg_ + 128),  // ink and paper swapped
        28, 0, 1, (char) scr->barW_, 1,             // a viewport over row 1
        12, 26,                                     // cleared, and put back
    };
    mos_puts((char*) paint, sizeof(paint), 0);
    scr_bar_line(scr, 1, bar, menu_bar_text(scr, bar, (int) sizeof(bar)));
}

void menu_attach(editor* ed) {
    open_menu = -1;
    scr_set_subheader(&ed->scr_, draw_bar, NULL);
}

void menu_start(editor* ed) {
    scr_header_draw(&ed->scr_);
}

// Where the pulled-down menu was last drawn: see menu_box.
static view box;

// The pulled-down menu, in a view of its own under its name, so the rows it
// paints are its box and nothing else. Painted without the document's
// colouring: these rows are not the document's lines.
static void draw_menu(editor* ed, int m, int at) {
    screen* scr = &ed->scr_;
    const menu* mn = &MENUS[m];
    static char line[64];

    int w = 0;
    for (int i = 0; i < mn->n; i++) {
        const int iw = (int) (strlen(mn->items[i].label) + strlen(mn->items[i].keys));
        if (iw > w) {
            w = iw;
        }
    }
    w += 8;                                         // "| > " ... "  " ... " |"
    if (w > (int) sizeof(line)) {
        w = (int) sizeof(line);
    }
    scr_view_init(scr, &box);
    box.textX_ = (char) menu_col(m);
    if (box.textX_ + w > scr->barW_) {
        box.textX_ = (char) (scr->barW_ - w);
    }
    box.cols_ = w;
    box.bottomY_ = (char) (box.topY_ + mn->n + 2);

    view* was = scr->v_;
    const scr_colourer rows = scr->colour_;
    const scr_cell_colourer cells = scr->cellColour_;
    void* ctx = scr->colourCtx_;
    scr_set_colourer(scr, NULL, NULL, NULL);
    scr_set_view(scr, &box);

    // In the bar's colours, swapped from the document's, all but the chosen
    // item: that one in the document's own, a bar of them across the box.
    memset(line, '-', (size_t) w);
    line[0] = '+';
    line[w - 1] = '+';
    scr_write_line_sel(scr, box.topY_, line, w, 0, w);
    for (int i = 0; i < mn->n; i++) {
        int k = ui_put_at(line, 0, w, i == at ? "| > " : "|   ");
        k = ui_put_at(line, k, w, mn->items[i].label);
        k = ui_pad_to(line, k, w, w - 2 - (int) strlen(mn->items[i].keys));
        k = ui_put_at(line, k, w, mn->items[i].keys);
        k = ui_pad_to(line, k, w, w - 1);
        k = ui_put_at(line, k, w, "|");
        if (i == at) {
            scr_write_line(scr, (char) (box.topY_ + 1 + i), line, k);
        } else {
            scr_write_line_sel(scr, (char) (box.topY_ + 1 + i), line, k, 0, w);
        }
    }
    memset(line, '-', (size_t) w);
    line[0] = '+';
    line[w - 1] = '+';
    scr_write_line_sel(scr, (char) (box.topY_ + mn->n + 1), line, w, 0, w);

    scr_set_view(scr, was);
    scr_set_colourer(scr, rows, cells, ctx);
}

// Puts the document back over wherever a menu was, and the bar without one open.
static void close_menus(editor* ed) {
    open_menu = -1;
    cmd_restore_after_modal(ed, false);
}

void menu_open(editor* ed) {
    int m = 0;
    int at = 0;
    for (;;) {
        open_menu = m;
        scr_header_draw(&ed->scr_);
        draw_menu(ed, m, at);

        const key_press kp = ks_wait(ed->ui_.keys_);
        if (kp.vkey == VK_ESCAPE || kp.vkey == VK_F10) {
            close_menus(ed);
            return;
        }
        if (kp.vkey == VK_LEFT || kp.vkey == VK_RIGHT
                || kp.vkey == VK_KP_LEFT || kp.vkey == VK_KP_RIGHT) {
            const int step = (kp.vkey == VK_LEFT || kp.vkey == VK_KP_LEFT) ? -1 : 1;
            m = (m + step + NMENUS) % NMENUS;
            at = 0;
            // The menu being left is under what comes back first.
            open_menu = -1;
            cmd_restore_after_modal(ed, false);
            continue;
        }
        const pick_key act = ui_modal_move(kp.vkey, &at, MENUS[m].n - 1);
        if (act != PICK_TAKE) {
            continue;
        }

        const menu_item* it = &MENUS[m].items[at];
        close_menus(ed);
        if (it->cmd == NULL) {
            ui_message(&ed->ui_, &ed->scr_, (char*) it->later);
            cmd_restore_after_modal(ed, false);
            return;
        }
        // Run as its key would be: the selection, the command, the repaint.
        key_command kc = { it->cmd, { 0, VK_NONE }, 0, it->flags };
        ed_handle(ed, kc);
        return;
    }
}

const view* menu_box(void) {
    return &box;
}
