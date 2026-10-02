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

#include "project.h"

#include <agon/mos.h>
#include <stdio.h>
#include <string.h>

#include "ini.h"

#define TEXT_MAX 2048

static const char RULE_C[] = "acc -c %f -o %b.o -errors %e";
static const char RULE_S[] = "zap %f %b.o -f acc -c -e %e";
static const char RULE_LINK[] = "acc %o -o %n.bin -errors %e";
static const char RULE_RUN[] = "%n";

static char text[TEXT_MAX];

// FNV-1a, 32 bits, over s[0, n), carried on from `h`.
static unsigned long hash(unsigned long h, const char* s, int n) {
    for (int i = 0; i < n; i++) {
        h = (h ^ (unsigned char) s[i]) * 16777619UL;
    }

    return h & 0xFFFFFFFFUL;
}

// s[0, len) into out, as a string; false when it does not fit.
static bool copy(char* out, int max, const char* s, int len) {
    if (len >= max) {
        return false;
    }
    memcpy(out, s, (size_t) len);
    out[len] = 0;

    return true;
}

// The extension of `name`, lower-cased into ext, or "" for none.
static void extension(const char* name, char* ext, int max) {
    const char* dot = strrchr(name, '.');
    ext[0] = 0;
    if (dot == NULL || strchr(dot, '/') != NULL) {
        return;
    }
    int i = 0;
    for (const char* c = dot + 1; *c != 0 && i < max - 1; c++) {
        ext[i++] = (char) (*c >= 'A' && *c <= 'Z' ? *c - 'A' + 'a' : *c);
    }
    ext[i] = 0;
}

// The command that builds `file`, or NULL when the project has none for it.
static const char* rule_for(const project* p, const char* file) {
    char ext[5];
    extension(file, ext, (int) sizeof(ext));
    if (strcmp(ext, "c") == 0) {
        return p->c;
    }
    if (strcmp(ext, "s") == 0) {
        return p->s;
    }
    if (strcmp(ext, "asm") == 0) {
        return p->asm_;
    }

    return NULL;
}

// The names in `s[0, len)`, split at spaces and commas, into `list` after
// the *n already there, at most `most` of them. `what` names the setting for
// a message.
static bool read_names(char list[][PROJ_NAME_MAX], int* n, int most, const char* s,
                       int len, const char* what, char* msg, int max) {
    int i = 0;
    while (i < len) {
        while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == ',')) {
            i++;
        }
        const int b = i;
        while (i < len && s[i] != ' ' && s[i] != '\t' && s[i] != ',') {
            i++;
        }
        if (i == b) {
            break;
        }
        if (*n == most) {
            snprintf(msg, (size_t) max, PROJ_FILE ": at most %d %s", most, what);
            return false;
        }
        if (!copy(list[*n], PROJ_NAME_MAX, s + b, i - b)) {
            snprintf(msg, (size_t) max, PROJ_FILE ": a name in %s is too long", what);
            return false;
        }
        (*n)++;
    }

    return true;
}

// Whether `name` is one of the project's excluded files, case aside: FAT
// names are not told apart by case.
static bool excluded(const project* p, const char* name) {
    for (int i = 0; i < p->nexclude; i++) {
        const char* a = p->exclude[i];
        const char* b = name;
        while (*a != 0 && *b != 0
               && (*a | 0x20) == (*b | 0x20)) {
            a++;
            b++;
        }
        if (*a == 0 && *b == 0) {
            return true;
        }
    }

    return false;
}

// With no files line, the project is every file in the directory that a rule
// builds, but those excluded, in order of name: a directory lists in whatever
// order it was written in, and the link should not depend on that.
static bool scan_directory(project* p, char* msg, int max) {
    // Static: a FILINFO carries a 256-byte name.
    static DIR dir;
    static FILINFO info;
    if (ffs_dopen(&dir, ".") != 0) {
        snprintf(msg, (size_t) max, "The directory could not be read");
        return false;
    }
    bool ok = true;
    while (ok) {
        if (ffs_dread(&dir, &info) != 0 || info.fname[0] == 0) {
            break;
        }
        if ((info.fattrib & AM_DIR) != 0 || rule_for(p, info.fname) == NULL
                || excluded(p, info.fname)) {
            continue;
        }
        const int len = (int) strlen(info.fname);
        if (p->nfiles == PROJ_FILES) {
            snprintf(msg, (size_t) max,
                     "More than %d files to build here: list them in " PROJ_FILE,
                     PROJ_FILES);
            ok = false;
        } else if (!copy(p->files[p->nfiles], PROJ_NAME_MAX, info.fname, len)) {
            snprintf(msg, (size_t) max, "%.40s: the name is too long", info.fname);
            ok = false;
        } else {
            // Into its place by name, the ones after it moved along one.
            int at = p->nfiles++;
            while (at > 0 && strcmp(p->files[at - 1], p->files[at]) > 0) {
                static char t[PROJ_NAME_MAX];   // static: off the frame
                memcpy(t, p->files[at], sizeof(t));
                memcpy(p->files[at], p->files[at - 1], sizeof(t));
                memcpy(p->files[at - 1], t, sizeof(t));
                at--;
            }
        }
    }
    ffs_dclose(&dir);

    return ok;
}

int proj_load(project* p, char* msg, int max) {
    const char fh = mos_fopen(PROJ_FILE, FA_READ);
    if (fh == 0) {
        return 0;
    }
    const int n = (int) mos_fread(fh, text, sizeof(text));
    mos_fclose(fh);
    if (n >= (int) sizeof(text)) {
        snprintf(msg, (size_t) max, PROJ_FILE " is longer than %d bytes", TEXT_MAX - 1);
        return -1;
    }

    memset(p, 0, sizeof(*p));
    // The file and ade's defaults: either changing makes a new build.obey.
    p->sum = hash(2166136261UL, text, n);
    p->sum = hash(p->sum, RULE_C, (int) sizeof(RULE_C));
    p->sum = hash(p->sum, RULE_S, (int) sizeof(RULE_S));
    p->sum = hash(p->sum, RULE_LINK, (int) sizeof(RULE_LINK));
    strcpy(p->c, RULE_C);
    strcpy(p->s, RULE_S);
    strcpy(p->asm_, RULE_S);
    strcpy(p->link, RULE_LINK);
    strcpy(p->run, RULE_RUN);

    const char* section = "";
    int seclen = 0;
    int i = 0;
    while (i < n) {
        const int start = i;
        while (i < n && text[i] != '\n') {
            i++;
        }
        const line ln = ini_read_line(text, start, i);
        i++;
        if (ln.kind == LINE_SECTION) {
            section = ln.name;
            seclen = ln.namelen;
            continue;
        }
        if (ln.kind != LINE_SETTING) {
            continue;
        }

        // Names ade does not know are left alone: a later version's, perhaps.
        bool fits = true;
        if (ini_name_is(section, seclen, "project")) {
            if (ini_name_is(ln.name, ln.namelen, "name")) {
                fits = copy(p->name, PROJ_NAME_MAX, ln.value, ln.valuelen);
            } else if (ini_name_is(ln.name, ln.namelen, "run")) {
                fits = copy(p->run, PROJ_CMD_MAX + 1, ln.value, ln.valuelen);
            } else if (ini_name_is(ln.name, ln.namelen, "files")) {
                p->listed = true;
                if (!read_names(p->files, &p->nfiles, PROJ_FILES, ln.value,
                                ln.valuelen, "files", msg, max)) {
                    return -1;
                }
            } else if (ini_name_is(ln.name, ln.namelen, "exclude")) {
                if (!read_names(p->exclude, &p->nexclude, PROJ_EXCLUDE, ln.value,
                                ln.valuelen, "excluded files", msg, max)) {
                    return -1;
                }
            }
        } else if (ini_name_is(section, seclen, "build")) {
            char* rule = NULL;
            if (ini_name_is(ln.name, ln.namelen, "c")) {
                rule = p->c;
            } else if (ini_name_is(ln.name, ln.namelen, "s")) {
                rule = p->s;
            } else if (ini_name_is(ln.name, ln.namelen, "asm")) {
                rule = p->asm_;
            } else if (ini_name_is(ln.name, ln.namelen, "link")) {
                rule = p->link;
            }
            if (rule != NULL) {
                fits = copy(rule, PROJ_CMD_MAX + 1, ln.value, ln.valuelen);
            }
        }
        if (!fits) {
            snprintf(msg, (size_t) max, PROJ_FILE ": %.*s is too long",
                     ln.namelen, ln.name);
            return -1;
        }
    }

    if (p->name[0] == 0) {
        snprintf(msg, (size_t) max, PROJ_FILE " names no program: name = ...");
        return -1;
    }
    // No files line: the directory is the project. The names it found go
    // into the sum, so a file added or taken away makes a new build.obey.
    if (!p->listed) {
        if (!scan_directory(p, msg, max)) {
            return -1;
        }
        for (int f = 0; f < p->nfiles; f++) {
            p->sum = hash(p->sum, p->files[f], (int) strlen(p->files[f]) + 1);
        }
        if (p->nfiles == 0) {
            snprintf(msg, (size_t) max, "No .c, .s or .asm files here to build");
            return -1;
        }
    }
    if (p->nfiles == 0) {
        snprintf(msg, (size_t) max, PROJ_FILE " lists no files: files = ...");
        return -1;
    }
    for (int f = 0; f < p->nfiles; f++) {
        if (rule_for(p, p->files[f]) == NULL) {
            snprintf(msg, (size_t) max, PROJ_FILE ": nothing builds %s", p->files[f]);
            return -1;
        }
    }

    return 1;
}

int proj_jobs(const project* p) {
    return p->nfiles + (p->link[0] != 0 ? 1 : 0);
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
        case 'o':
            for (int f = 0; f < p->nfiles && ok; f++) {
                ok = (f == 0 || put(out, max, &at, " ", 1))
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

    return expand(p, i < p->nfiles ? rule_for(p, file) : p->link, file, err, out, max);
}

bool proj_run(const project* p, char* out, int max) {
    return expand(p, p->run, "", "", out, max);
}

bool proj_obey(const project* p, const char* err, char* msg, int max) {
    static char head[80];
    const int hn = snprintf(head, sizeof(head),
                            "# Made by ade from " PROJ_FILE " %08lx: edits last until it changes\r\n",
                            p->sum);

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
            snprintf(msg, (size_t) max, "Command %d of " PROJ_FILE " is longer than %d",
                     i + 1, PROJ_CMD_MAX);
            return false;
        }
    }
    fh = mos_fopen(PROJ_OBEY, FA_WRITE | FA_CREATE_ALWAYS);
    if (fh == 0) {
        snprintf(msg, (size_t) max, PROJ_OBEY " could not be written");
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
        snprintf(msg, (size_t) max, PROJ_OBEY " could not be written");
    }

    return ok;
}

bool proj_create(const char* name, char* msg, int max) {
    const char was = mos_fopen(PROJ_FILE, FA_READ);
    if (was != 0) {
        mos_fclose(was);
        snprintf(msg, (size_t) max, PROJ_FILE " is here already: F9 builds from it");
        return false;
    }
    static char fresh[1024];
    const int n = snprintf(fresh, sizeof(fresh),
        "; An ade project. F9 builds every .c, .s and .asm file in this\r\n"
        "; directory and links them into %s.bin; CTRL+F9 builds it and runs it.\r\n"
        "\r\n"
        "[project]\r\n"
        "name = %s\r\n"
        "\r\n"
        "; Only some of the files, in this order, instead of all of them:\r\n"
        ";files = main.c util.c\r\n"
        "; All of them but these:\r\n"
        ";exclude = scratch.c\r\n"
        "; How CTRL+F9 runs the program, %%n being its name:\r\n"
        ";run = %%n\r\n"
        "\r\n"
        "; How each kind of file is built, and the link. These are what ade\r\n"
        "; does without them: %%f is the file, %%b the file less its extension,\r\n"
        "; %%o every object, %%n the program, %%e the file errors go to.\r\n"
        ";[build]\r\n"
        ";c = %s\r\n"
        ";s = %s\r\n"
        ";asm = %s\r\n"
        ";link = %s\r\n",
        name, name, RULE_C, RULE_S, RULE_S, RULE_LINK);
    if (n <= 0 || n >= (int) sizeof(fresh)) {
        snprintf(msg, (size_t) max, "The program's name is too long");
        return false;
    }
    const char fh = mos_fopen(PROJ_FILE, FA_WRITE | FA_CREATE_NEW);
    if (fh == 0) {
        snprintf(msg, (size_t) max, PROJ_FILE " could not be written");
        return false;
    }
    const bool ok = (int) mos_fwrite(fh, fresh, (unsigned) n) == n;
    mos_fclose(fh);
    if (!ok) {
        mos_del(PROJ_FILE);
        snprintf(msg, (size_t) max, PROJ_FILE " could not be written");
    }

    return ok;
}
