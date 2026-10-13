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
#include <stdlib.h>
#include <string.h>

#include "ini.h"
#include "project_int.h"
#include "text.h"

#define TEXT_MAX 2048

static const char RULE_C[] = RULE_C_TEXT;
static const char RULE_S[] = RULE_S_TEXT;
static const char RULE_LINK[] = RULE_LINK_TEXT;
static const char RULE_RUN[] = RULE_RUN_TEXT;
static const char RULE_DOC[] = RULE_DOC_TEXT;

static char text_[TEXT_MAX];

// What went wrong, into msg: up to three pieces, end to end; NULL for none.
void proj_fail(char* msg, int max, const char* a, const char* b, const char* c) {
    text t;
    text_start(&t, msg, max);
    text_str(&t, a);
    if (b != NULL) {
        text_str(&t, b);
    }
    if (c != NULL) {
        text_str(&t, c);
    }
}

// FNV-1a, 32 bits, over s[0, n), carried on from `h`.
unsigned long proj_hash(unsigned long h, const char* s, int n) {
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
const char* proj_rule_for(const project* p, const char* file) {
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
            text t;
            text_start(&t, msg, max);
            text_str(&t, PROJ_FILE ": at most ");
            text_int(&t, most);
            text_str(&t, " ");
            text_str(&t, what);
            return false;
        }
        if (!copy(list[*n], PROJ_NAME_MAX, s + b, i - b)) {
            proj_fail(msg, max, PROJ_FILE ": a name in ", what, " is too long");
            return false;
        }
        (*n)++;
    }

    return true;
}

// Whether `name` is one of the project's excluded files, case aside: FAT
// names are not told apart by case.
bool proj_excluded(const project* p, const char* name) {
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

// The document's name, from the program's: what ref takes, 1 to 8 lower-case
// letters and digits.
static void doc_name(project* p) {
    int n = 0;
    for (const char* c = p->name; *c != 0 && n < PROJ_DOC_NAME; c++) {
        const char l = (char) (*c >= 'A' && *c <= 'Z' ? *c - 'A' + 'a' : *c);
        if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9')) {
            p->doc[n++] = l;
        }
    }
    if (n == 0) {
        memcpy(p->doc, "doc", 4);
        return;
    }
    p->doc[n] = 0;
}

int proj_load(project* p, char* msg, int max) {
    const char fh = mos_fopen(PROJ_FILE, FA_READ);
    if (fh == 0) {
        return 0;
    }
    const int n = (int) mos_fread(fh, text_, sizeof(text_));
    mos_fclose(fh);
    if (n >= (int) sizeof(text_)) {
        text t;
        text_start(&t, msg, max);
        text_str(&t, PROJ_FILE " is longer than ");
        text_int(&t, TEXT_MAX - 1);
        text_str(&t, " bytes");
        return -1;
    }

    memset(p, 0, sizeof(*p));
    // The file and ade's defaults: either changing makes a new build.obey.
    p->sum = proj_hash(2166136261UL, text_, n);
    p->sum = proj_hash(p->sum, RULE_C, (int) sizeof(RULE_C));
    p->sum = proj_hash(p->sum, RULE_S, (int) sizeof(RULE_S));
    p->sum = proj_hash(p->sum, RULE_LINK, (int) sizeof(RULE_LINK));
    p->sum = proj_hash(p->sum, RULE_DOC, (int) sizeof(RULE_DOC));
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
        while (i < n && text_[i] != '\n') {
            i++;
        }
        const line ln = ini_read_line(text_, start, i);
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
            text t;
            text_start(&t, msg, max);
            text_str(&t, PROJ_FILE ": ");
            text_strn(&t, ln.name, ln.namelen);
            text_str(&t, " is too long");
            return -1;
        }
    }

    if (p->name[0] == 0) {
        proj_fail(msg, max, PROJ_FILE " names no program: name = ...", NULL, NULL);
        return -1;
    }
    // No files line: the directory is the project. The names it found go
    // into the sum, so a file added or taken away makes a new build.obey --
    // and so do the documentation's, which are looked for either way.
    if (!proj_scan(p, !p->listed, msg, max)) {
        return -1;
    }
    for (const char* d = proj_next_doc(p, NULL); d != NULL; d = proj_next_doc(p, d)) {
        p->sum = proj_hash(p->sum, d, (int) strlen(d) + 1);
    }
    doc_name(p);
    if (!p->listed) {
        for (int f = 0; f < p->nfiles; f++) {
            p->sum = proj_hash(p->sum, p->files[f], (int) strlen(p->files[f]) + 1);
        }
        if (p->nfiles == 0) {
            proj_fail(msg, max, "No .c, .s or .asm files here to build", NULL, NULL);
            return -1;
        }
    }
    if (p->nfiles == 0) {
        proj_fail(msg, max, PROJ_FILE " lists no files: files = ...", NULL, NULL);
        return -1;
    }
    for (int f = 0; f < p->nfiles; f++) {
        if (proj_rule_for(p, p->files[f]) == NULL) {
            proj_fail(msg, max, PROJ_FILE ": nothing builds ", p->files[f], NULL);
            return -1;
        }
    }

    return 1;
}

bool proj_rename(const char* name, char* msg, int max) {
    char fh = mos_fopen(PROJ_FILE, FA_READ);
    if (fh == 0) {
        proj_fail(msg, max, PROJ_FILE " could not be read", NULL, NULL);
        return false;
    }
    const int n = (int) mos_fread(fh, text_, TEXT_MAX);
    mos_fclose(fh);
    if (n <= 0 || n == TEXT_MAX) {
        proj_fail(msg, max, PROJ_FILE " could not be read", NULL, NULL);
        return false;
    }

    // The name line of [project]: where it starts and where its text ends,
    // before the line's end -- which is kept, as is everything around it.
    int from = -1;
    int to = -1;
    bool in_project = false;
    for (int at = 0; at < n && from < 0;) {
        int end = at;
        while (end < n && text_[end] != '\r' && text_[end] != '\n') {
            end++;
        }
        int k = at;
        while (k < end && (text_[k] == ' ' || text_[k] == '\t')) {
            k++;
        }
        if (k < end && text_[k] == '[') {
            in_project = end - k >= 9 && memcmp(text_ + k, "[project]", 9) == 0;
        } else if (in_project && end - k >= 4 && memcmp(text_ + k, "name", 4) == 0) {
            int e = k + 4;
            while (e < end && (text_[e] == ' ' || text_[e] == '\t')) {
                e++;
            }
            if (e < end && text_[e] == '=') {
                from = at;
                to = end;
            }
        }
        at = end;
        while (at < n && (text_[at] == '\r' || text_[at] == '\n')) {
            at++;
        }
    }
    if (from < 0) {
        proj_fail(msg, max, PROJ_FILE " has no name line to change", NULL, NULL);
        return false;
    }

    fh = mos_fopen(PROJ_FILE, FA_WRITE | FA_CREATE_ALWAYS);
    if (fh == 0) {
        proj_fail(msg, max, PROJ_FILE " could not be written", NULL, NULL);
        return false;
    }
    const int len = (int) strlen(name);
    bool ok = (int) mos_fwrite(fh, text_, (unsigned) from) == from
              && mos_fwrite(fh, (char*) "name = ", 7) == 7
              && (int) mos_fwrite(fh, (char*) name, (unsigned) len) == len
              && (int) mos_fwrite(fh, text_ + to, (unsigned) (n - to)) == n - to;
    mos_fclose(fh);
    if (!ok) {
        proj_fail(msg, max, PROJ_FILE " could not be written", NULL, NULL);
    }

    return ok;
}

bool proj_create(const char* name, char* msg, int max) {
    const char was = mos_fopen(PROJ_FILE, FA_READ);
    if (was != 0) {
        mos_fclose(was);
        proj_fail(msg, max, PROJ_FILE " is here already: CTRL+B builds from it", NULL, NULL);
        return false;
    }
    static char fresh[1024];
    text t;
    text_start(&t, fresh, (int) sizeof(fresh));
    text_str(&t,
        "; An ade project. CTRL+B builds every .c, .s and .asm file in this\r\n"
        "; directory and those under it, into build/, and links them into bin/");
    text_str(&t, name);
    text_str(&t, ".bin; CTRL+R\r\n"
        "; builds it and runs it.\r\n"
        "\r\n"
        "[project]\r\n"
        "name = ");
    text_str(&t, name);
    text_str(&t, "\r\n"
        "\r\n"
        "; Only some of the files, in this order, instead of all of them:\r\n"
        ";files = main.c util.c\r\n"
        "; All of them but these:\r\n"
        ";exclude = scratch.c\r\n"
        "; How CTRL+R runs the program, %n being its name:\r\n"
        ";run = " RULE_RUN_TEXT "\r\n"
        "\r\n"
        "; How each kind of file is built, and the link. These are what ade\r\n"
        "; does without them: %f is the file, %b the file less its extension,\r\n"
        "; %o every object, as build/%b.o, %l a file listing them, build/%n.lnk,\r\n"
        "; for acc's @, %n the program, %e the file errors go to.\r\n"
        ";[build]\r\n"
        ";c = " RULE_C_TEXT "\r\n"
        ";s = " RULE_S_TEXT "\r\n"
        ";asm = " RULE_S_TEXT "\r\n"
        ";link = " RULE_LINK_TEXT "\r\n");
    const int n = text_len(&t);
    if (t.cut) {
        proj_fail(msg, max, "The program's name is too long", NULL, NULL);
        return false;
    }
    const char fh = mos_fopen(PROJ_FILE, FA_WRITE | FA_CREATE_NEW);
    if (fh == 0) {
        proj_fail(msg, max, PROJ_FILE " could not be written", NULL, NULL);
        return false;
    }
    const bool ok = (int) mos_fwrite(fh, fresh, (unsigned) n) == n;
    mos_fclose(fh);
    if (!ok) {
        mos_del(PROJ_FILE);
        proj_fail(msg, max, PROJ_FILE " could not be written", NULL, NULL);
    }

    return ok;
}

// What is known of the project here between reads -- see proj_here_name.
// `loaded` is proj_load's answer, and 2 for not read yet.
static int loaded = 2;
static char known_name[PROJ_NAME_MAX];
static char known_first[PROJ_NAME_MAX];
static char known_doc[PROJ_DOC_NAME + 1];

project* proj_read(int* has, char* msg, int max) {
    project* p = malloc(sizeof(project));
    if (p == NULL) {
        proj_fail(msg, max, "No room to read " PROJ_FILE, NULL, NULL);
        loaded = 2;
        return NULL;
    }
    *has = proj_load(p, msg, max);
    loaded = *has;
    if (*has > 0) {
        strcpy(known_name, p->name);
        strcpy(known_first, p->files[0]);
        strcpy(known_doc, p->ndocs > 0 ? p->doc : "");
    }

    return p;
}

// Reads the project here, if it has not been, for what is kept of it.
static void know(void) {
    if (loaded == 2) {
        static char msg[80];
        int has = 0;
        free(proj_read(&has, msg, (int) sizeof(msg)));
    }
}

const char* proj_here_name(void) {
    know();

    return loaded > 0 ? known_name : NULL;
}

const char* proj_here_first(void) {
    know();

    return loaded > 0 ? known_first : NULL;
}

const char* proj_here_doc(void) {
    know();

    return loaded > 0 ? known_doc : "";
}

void proj_forget(void) {
    loaded = 2;
}
