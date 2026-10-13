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

// Writing a project's build: build.obey, the commands hub runs with MOS's
// Obey; the lists the link and md2ref read; and the directories they write
// into.

#include <agon/mos.h>
#include <string.h>

#include "project_int.h"
#include "text.h"

static const char RULE_DOC[] = RULE_DOC_TEXT;

int proj_jobs(const project* p) {
    return p->nfiles + (p->link[0] != 0 ? 1 : 0) + (p->ndocs > 0 ? 1 : 0);
}

// Appends s[0, len) at out + *at; false when it does not fit in max.
static bool put(char* out, int max, int* at, const char* s, int len) {
    if (*at + len >= max) {
        return false;
    }
    memcpy(out + *at, s, (size_t) len);
    *at += len;

    return true;
}

// The length of `file` without its extension.
static int base_len(const char* file) {
    const char* dot = strrchr(file, '.');
    if (dot == NULL || strchr(dot, '/') != NULL) {
        return (int) strlen(file);
    }

    return (int) (dot - file);
}

// `rule` with its % names put in, for `file` -- "" for the link and the run.
static bool expand(const project* p, const char* rule, const char* file,
                   const char* err, char* out, int max) {
    int at = 0;
    bool ok = true;
    for (const char* r = rule; *r != 0 && ok; r++) {
        if (r[0] != '%' || r[1] == 0) {
            ok = put(out, max, &at, r, 1);
            continue;
        }
        r++;
        switch (*r) {
        case 'f':
            ok = put(out, max, &at, file, (int) strlen(file));
            break;
        case 'b':
            ok = put(out, max, &at, file, base_len(file));
            break;
        case 'n':
            ok = put(out, max, &at, p->name, (int) strlen(p->name));
            break;
        case 'e':
            ok = put(out, max, &at, err, (int) strlen(err));
            break;
        case 'd':
            ok = put(out, max, &at, p->doc, (int) strlen(p->doc));
            break;
        case 'l':
            ok = put(out, max, &at, PROJ_OBJ_DIR "/", (int) sizeof(PROJ_OBJ_DIR "/") - 1)
                 && put(out, max, &at, p->name, (int) strlen(p->name))
                 && put(out, max, &at, PROJ_LIST_EXT, (int) sizeof(PROJ_LIST_EXT) - 1);
            break;
        case 'o':
            for (int f = 0; f < p->nfiles && ok; f++) {
                ok = (f == 0 || put(out, max, &at, " ", 1))
                     && put(out, max, &at, PROJ_OBJ_DIR "/", (int) sizeof(PROJ_OBJ_DIR "/") - 1)
                     && put(out, max, &at, p->files[f], base_len(p->files[f]))
                     && put(out, max, &at, ".o", 2);
            }
            break;
        default:                        // %% and anything unknown: as written
            ok = put(out, max, &at, r, 1);
            break;
        }
    }
    out[at < max ? at : max - 1] = 0;

    return ok;
}

bool proj_job(const project* p, int i, const char* err, char* out, int max) {
    const char* file = i < p->nfiles ? p->files[i] : "";
    const char* rule = i < p->nfiles ? proj_rule_for(p, file) : p->link;
    // The documentation last, after the link -- or in its place, with none.
    if (i == proj_jobs(p) - 1 && p->ndocs > 0) {
        rule = RULE_DOC;
    }

    return expand(p, rule, file, err, out, max);
}

bool proj_run(const project* p, char* out, int max) {
    return expand(p, p->run, "", "", out, max);
}

bool proj_obey(const project* p, const char* err, char* msg, int max) {
    static char head[80];
    text h;
    text_start(&h, head, (int) sizeof(head));
    text_str(&h, "# Made by ade from " PROJ_FILE " ");
    // The error file is in every command too, so it is in the sum: a
    // build.obey writing errors somewhere else is made again.
    text_hex(&h, proj_hash(p->sum, err, (int) strlen(err)), 8);
    text_str(&h, ": edits last until it changes\r\n");
    const int hn = text_len(&h);

    // Made from this project.ini already: left as it is.
    static char was[80];
    char fh = mos_fopen(PROJ_OBEY, FA_READ);
    if (fh != 0) {
        const int n = (int) mos_fread(fh, was, (unsigned) hn);
        mos_fclose(fh);
        if (n == hn && memcmp(was, head, (size_t) hn) == 0) {
            return true;
        }
    }

    static char cmd[PROJ_CMD_MAX + 3];
    const int jobs = proj_jobs(p);
    for (int i = 0; i < jobs; i++) {
        if (!proj_job(p, i, err, cmd, PROJ_CMD_MAX + 1)) {
            text t;
            text_start(&t, msg, max);
            text_str(&t, "Command ");
            text_int(&t, i + 1);
            text_str(&t, " of " PROJ_FILE " is longer than ");
            text_int(&t, PROJ_CMD_MAX);
            return false;
        }
    }
    fh = mos_fopen(PROJ_OBEY, FA_WRITE | FA_CREATE_ALWAYS);
    if (fh == 0) {
        proj_fail(msg, max, PROJ_OBEY " could not be written", NULL, NULL);
        return false;
    }
    bool ok = (int) mos_fwrite(fh, head, (unsigned) hn) == hn;
    for (int i = 0; i < jobs && ok; i++) {
        proj_job(p, i, err, cmd, PROJ_CMD_MAX + 1);
        const int n = (int) strlen(cmd);
        cmd[n] = '\r';
        cmd[n + 1] = '\n';
        ok = (int) mos_fwrite(fh, cmd, (unsigned) n + 2) == n + 2;
    }
    mos_fclose(fh);
    if (!ok) {
        mos_del(PROJ_OBEY);             // half a build is worse than none
        proj_fail(msg, max, PROJ_OBEY " could not be written", NULL, NULL);
    }

    return ok;
}

bool proj_list(const project* p, char* msg, int max) {
    if (p->link[0] == 0) {
        return true;
    }
    static char path[PROJ_NAME_MAX + 16];
    if (!expand(p, "%l", "", "", path, (int) sizeof(path))) {
        proj_fail(msg, max, "The program's name is too long", NULL, NULL);
        return false;
    }
    const char fh = mos_fopen(path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fh == 0) {
        proj_fail(msg, max, path, " could not be written", NULL);
        return false;
    }
    static char line[PROJ_NAME_MAX + 16];
    bool ok = true;
    for (int f = 0; f < p->nfiles && ok; f++) {
        text t;
        text_start(&t, line, (int) sizeof(line));
        text_str(&t, PROJ_OBJ_DIR "/");
        text_strn(&t, p->files[f], base_len(p->files[f]));
        text_str(&t, ".o\r\n");
        const int n = text_len(&t);
        ok = (int) mos_fwrite(fh, line, (unsigned) n) == n;
    }
    mos_fclose(fh);
    if (!ok) {
        mos_del(path);
        proj_fail(msg, max, path, " could not be written", NULL);
    }

    return ok;
}

bool proj_docs_list(const project* p, char* msg, int max) {
    if (p->ndocs == 0) {
        return true;
    }
    static char path[PROJ_NAME_MAX];
    if (!expand(p, PROJ_OBJ_DIR "/%d" PROJ_DOCS_EXT, "", "", path, (int) sizeof(path))) {
        proj_fail(msg, max, "The document's name is too long", NULL, NULL);
        return false;
    }
    const char fh = mos_fopen(path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fh == 0) {
        proj_fail(msg, max, path, " could not be written", NULL);
        return false;
    }
    bool ok = true;
    for (const char* d = proj_next_doc(p, NULL); d != NULL && ok; d = proj_next_doc(p, d)) {
        const int n = (int) strlen(d);
        ok = (int) mos_fwrite(fh, (char*) d, (unsigned) n) == n
             && mos_fwrite(fh, (char*) "\r\n", 2) == 2;
    }
    mos_fclose(fh);
    if (!ok) {
        mos_del(path);
        proj_fail(msg, max, path, " could not be written", NULL);
    }

    return ok;
}


void proj_dirs(const project* p) {
    // A directory that is there already is the usual case, and not an error.
    mos_mkdir(PROJ_OBJ_DIR);
    mos_mkdir(PROJ_BIN_DIR);

    // And build/more, build/more/deeper, for a file in more/deeper: the tools
    // write an object where they are told, and make no directories for it.
    static char dir[PROJ_NAME_MAX + 8];
    for (int f = 0; f < p->nfiles; f++) {
        const char* name = p->files[f];
        for (const char* slash = strchr(name, '/'); slash != NULL;
             slash = strchr(slash + 1, '/')) {
            text t;
            text_start(&t, dir, (int) sizeof(dir));
            text_str(&t, PROJ_OBJ_DIR "/");
            text_strn(&t, name, (int) (slash - name));
            if (!t.cut) {
                mos_mkdir(dir);
            }
        }
    }
}

