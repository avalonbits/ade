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

#include "lookup.h"

#include <agon/mos.h>
#include <string.h>

#include <hub/hub.h>

#include "away.h"
#include "project.h"
#include "say.h"
#include "text.h"

#define DOC_ACC "acc"
#define DOC_MOS "mos"
#define DOCS_DIR "/config/ref/docs/"

// What ref exits with when nothing matched.
#define REF_NOTHING 100

// Whether c can be part of a word to look up: mos_fopen, run$path, f_printf.
static bool word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
           || c == '_' || c == '$';
}

// The word to look up, into out: the selection, when it is on one line -- "vdu
// 23,27,3" -- or else the word the cursor is in or just after. Empty for none.
static void word_at_cursor(editor* ed, char* out, int max) {
    text_buffer* tb = &ed->doc_->buf_;
    int psz = 0;
    int ssz = 0;
    const char* before = tb_prefix(tb, &psz);       // the line up to the cursor
    const char* after = tb_suffix(tb, &ssz);        // ...and from it
    int from = psz;
    int to = 0;
    const document* d = ed->doc_;
    const tb_pos at = tb_tell(tb);

    if (d->selecting_ && d->anchor_.line == at.line && d->anchor_.x != at.x) {
        // The selection: from the anchor to the cursor, whichever side it is.
        if (d->anchor_.x < at.x) {
            from = psz - (at.x - d->anchor_.x);
            from = from < 0 ? 0 : from;
        } else {
            to = d->anchor_.x - at.x;
            to = to > ssz ? ssz : to;
        }
    } else {
        while (from > 0 && word_char(before[from - 1])) {
            from--;
        }
        while (to < ssz && word_char(after[to])) {
            to++;
        }
    }

    text t;
    text_start(&t, out, max);
    text_strn(&t, before + from, psz - from);
    text_strn(&t, after, to);

    // Blanks and line ends at either side are not part of it.
    int n = text_len(&t);
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\r' || out[n - 1] == '\n')) {
        out[--n] = 0;
    }
    int skip = 0;
    while (out[skip] == ' ') {
        skip++;
    }
    memmove(out, out + skip, (size_t) (n - skip + 1));
}

// acc's document for a .c or .h file, the Agon's for anything else.
static const char* document_for(editor* ed) {
    const char* name = tb_fname(&ed->doc_->buf_);
    const char* dot = name != NULL ? strrchr(name, '.') : NULL;
    if (dot != NULL && strchr(dot, '/') == NULL && dot[1] != 0 && dot[2] == 0) {
        const char x = (char) (dot[1] | 0x20);
        if (x == 'c' || x == 'h') {
            return DOC_ACC;
        }
    }

    return DOC_MOS;
}

// Whether document `doc` is on the card. ref exits 100 both for a word it has
// nothing for and for a missing document, so ade looks first, and says which.
static bool document_there(editor* ed, const char* doc) {
    static char path[32];
    text t;
    text_start(&t, path, (int) sizeof(path));
    text_str(&t, DOCS_DIR);
    text_str(&t, doc);
    text_str(&t, ".dat");
    const char fh = mos_fopen(path, FA_READ);
    if (fh != 0) {
        mos_fclose(fh);
        return true;
    }
    static char msg[64];
    text_start(&t, msg, (int) sizeof(msg));
    text_str(&t, "ref has no ");
    text_str(&t, doc);
    text_str(&t, " document: ");
    text_str(&t, path);
    say(ed, msg);

    return false;
}

// Runs ref on `word` -- "" for the contents -- in `doc`, after the project's
// own document when there is one.
static void run_ref(editor* ed, const char* doc, const char* word) {
    if (!document_there(ed, doc)) {
        return;
    }
    const char* own = word[0] != 0 && proj_here_name() != NULL ? proj_here_doc() : "";

    // ref ref-doc/widgets1.dat,acc -k fopen: ref tries the documents in turn,
    // skipping one that is not there yet.
    static char cmd[HUB_CMD_MAX + 1];
    text t;
    text_start(&t, cmd, (int) sizeof(cmd));
    text_str(&t, "ref ");
    if (own[0] != 0) {
        text_str(&t, PROJ_DOC_DIR "/");
        text_str(&t, own);
        text_str(&t, ".dat,");
    }
    text_str(&t, doc);
    if (word[0] != 0) {
        text_str(&t, " -k ");
        text_str(&t, word);
    }
    if (t.cut) {
        say(ed, "Too long a word for hub to run ref with");
        return;
    }

    if (!away_save_windows(ed)) {
        return;
    }
    session* s = away_keep(ed, SESSION_REF);
    if (s == NULL) {
        return;
    }
    strcpy(s->word, word);
    strcpy(s->doc, doc);
    strcpy(s->proj_doc, own);
    // ref on the screen hub's prompt has; ade back whatever it found.
    away_hand_over(ed, s, cmd, HUB_USER_PROGRAM, NULL, 0, "run ref");
}

void lookup_word(editor* ed) {
    static char word[SESSION_WORD_MAX + 1];
    word_at_cursor(ed, word, (int) sizeof(word));
    if (word[0] == 0) {
        say(ed, "No word at the cursor to look up in ref");
        return;
    }
    run_ref(ed, document_for(ed), word);
}

void lookup_contents(editor* ed) {
    run_ref(ed, DOC_MOS, "");
}

void lookup_report(editor* ed, const session* s) {
    const int result = hub_last_result();
    if (result == 0) {
        return;
    }
    if (result != REF_NOTHING) {
        say(ed, "Could not run ref: is /bin/ref.bin on the card?");
        return;
    }
    static char msg[SESSION_WORD_MAX + 48];
    text t;
    text_start(&t, msg, (int) sizeof(msg));
    if (s->word[0] == 0) {
        text_str(&t, "ref could not open its ");
        text_str(&t, s->doc);
        text_str(&t, " document");
    } else {
        text_str(&t, "Nothing in ref's ");
        if (s->proj_doc[0] != 0) {
            text_str(&t, s->proj_doc);
            text_str(&t, " or ");
        }
        text_str(&t, s->doc);
        text_str(&t, " for ");
        text_str(&t, s->word);
    }
    say(ed, msg);
}
