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

// Finding a project's files: the sources to build and the pages of its
// documentation, in its directory and those under it.

#include <agon/mos.h>
#include <string.h>

#include "project_int.h"
#include "text.h"

// How deep under the project's directory files are looked for.
#define SCAN_DEPTH 4

// Where scan_directory is, under the project's directory: "" there, else
// "more" or "more/deeper". One DIR for each level open at once.
static char scan_path[PROJ_NAME_MAX];
static DIR scan_dirs[SCAN_DEPTH];

// Adds `name`, a file under scan_path, into its place by its whole path. False
// with why in msg when it does not fit.
static bool scan_add(project* p, const char* name, char* msg, int max) {
    static char path[PROJ_NAME_MAX];
    text t;
    text_start(&t, path, PROJ_NAME_MAX);
    if (scan_path[0] != 0) {
        text_str(&t, scan_path);
        text_str(&t, "/");
    }
    text_str(&t, name);
    if (t.cut) {
        text_start(&t, msg, max);
        text_strn(&t, name, 40);
        text_str(&t, ": the name is too long");
        return false;
    }
    if (proj_excluded(p, path)) {
        return true;
    }
    if (p->nfiles == PROJ_FILES) {
        text_start(&t, msg, max);
        text_str(&t, "More than ");
        text_int(&t, PROJ_FILES);
        text_str(&t, " files to build here: list them in " PROJ_FILE);
        return false;
    }
    strcpy(p->files[p->nfiles], path);

    // Into its place by name, the ones after it moved along one.
    int at = p->nfiles++;
    while (at > 0 && strcmp(p->files[at - 1], p->files[at]) > 0) {
        static char swap[PROJ_NAME_MAX];   // static: off the frame
        memcpy(swap, p->files[at], sizeof(swap));
        memcpy(p->files[at], p->files[at - 1], sizeof(swap));
        memcpy(p->files[at - 1], swap, sizeof(swap));
        at--;
    }

    return true;
}

// Whether `name` is Markdown: ends in .md, in either case.
static bool markdown(const char* name) {
    const int n = (int) strlen(name);

    return n > 3 && name[n - 3] == '.' && (name[n - 2] | 0x20) == 'm'
           && (name[n - 1] | 0x20) == 'd';
}

// Adds `name`, a Markdown file under scan_path, to the documentation. One
// that does not fit is left out: the rest still make a document.
static void scan_doc(project* p, const char* name) {
    if (p->ndocs == PROJ_DOCS_MAX) {
        return;
    }
    text t;
    text_start(&t, p->docs + p->docs_used, PROJ_DOCS_POOL - p->docs_used);
    if (scan_path[0] != 0) {
        text_str(&t, scan_path);
        text_str(&t, "/");
    }
    text_str(&t, name);
    if (t.cut) {
        return;
    }
    p->docs_used += text_len(&t) + 1;
    p->ndocs++;
}

// The files under scan_path, and under the directories in it, to SCAN_DEPTH.
// Names starting with "." are left out, and so are the build's own build/
// and bin/ at the top.
static bool scan_level(project* p, int depth, bool sources, char* msg, int max) {
    // Static: a FILINFO carries a 256-byte name. Each name is used before
    // the next level reads into it.
    static FILINFO info;
    DIR* dir = &scan_dirs[depth];
    if (ffs_dopen(dir, scan_path[0] != 0 ? scan_path : ".") != 0) {
        proj_fail(msg, max, "The directory could not be read", NULL, NULL);
        return false;
    }
    bool ok = true;
    while (ok) {
        if (ffs_dread(dir, &info) != 0 || info.fname[0] == 0) {
            break;
        }
        if (info.fname[0] == '.') {
            continue;
        }
        if ((info.fattrib & AM_DIR) == 0) {
            if (markdown(info.fname)) {
                scan_doc(p, info.fname);
            } else if (sources && proj_rule_for(p, info.fname) != NULL) {
                ok = scan_add(p, info.fname, msg, max);
            }
            continue;
        }
        if (depth + 1 == SCAN_DEPTH
                || (depth == 0 && (strcmp(info.fname, PROJ_OBJ_DIR) == 0
                                   || strcmp(info.fname, PROJ_BIN_DIR) == 0
                                   || strcmp(info.fname, PROJ_DOC_DIR) == 0))) {
            continue;
        }
        // Down a level, and back.
        const int was = (int) strlen(scan_path);
        const int n = (int) strlen(info.fname);
        if (was + 1 + n >= PROJ_NAME_MAX) {
            continue;                   // too deep a name to keep: not looked in
        }
        if (was > 0) {
            scan_path[was] = '/';
        }
        memcpy(scan_path + (was > 0 ? was + 1 : 0), info.fname, (size_t) n + 1);
        ok = scan_level(p, depth + 1, sources, msg, max);
        scan_path[was] = 0;
    }
    ffs_dclose(dir);

    return ok;
}

// With no files line, the project is every file under its directory that a
// rule builds, but those excluded, in order of their paths -- "more/func.c"
// -- since a directory lists in whatever order it was written in, and the link
// should not depend on that. With one, only its documentation is looked for:
// every .md under the directory, whichever way the sources are named.
bool proj_scan(project* p, bool sources, char* msg, int max) {
    scan_path[0] = 0;

    return scan_level(p, 0, sources, msg, max);
}

// a and b compared as FAT compares names, without regard to case.
static int compare_ci(const char* a, const char* b) {
    for (;; a++, b++) {
        const char x = (char) (*a >= 'A' && *a <= 'Z' ? *a - 'A' + 'a' : *a);
        const char y = (char) (*b >= 'A' && *b <= 'Z' ? *b - 'A' + 'a' : *b);
        if (x != y || x == 0) {
            return (unsigned char) x - (unsigned char) y;
        }
    }
}

// Whether `a` comes before `b` among the pages: a README.md or index.md at the
// top first, then by path.
static bool doc_before(const char* a, const char* b) {
    const bool fa = compare_ci(a, "README.md") == 0 || compare_ci(a, "index.md") == 0;
    const bool fb = compare_ci(b, "README.md") == 0 || compare_ci(b, "index.md") == 0;
    if (fa != fb) {
        return fa;
    }

    return strcmp(a, b) < 0;
}

// The page after `prev` in the pages' order -- the first for NULL -- or NULL
// after the last. The pool is in the directory's order, which is not the
// document's; there are few enough pages to look through it each time.
const char* proj_next_doc(const project* p, const char* prev) {
    const char* best = NULL;
    const char* d = p->docs;
    for (int i = 0; i < p->ndocs; i++) {
        if ((prev == NULL || doc_before(prev, d)) && (best == NULL || doc_before(d, best))) {
            best = d;
        }
        d += strlen(d) + 1;
    }

    return best;
}

