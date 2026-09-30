/*
 * The keys a person would press, played on real MOS: which keys depends on how
 * ade was started. On a.txt, the windows and the menus, below. On a source
 * file, F9, to build it; and when hub brings ade back, -resume, leaving.
 *
 * The windows and the menus:
 *
 * The card holds a.txt ("alpha"), b.txt ("bravo") and c.txt ("charlie"), and
 * ade starts on a.txt. The run types in each window, opens the other two with
 * F3, is told that /a.txt is window 1's a.txt already and at the fourth that three
 * are open, moves with F6 and the ALT+F6
 * list, closes window 3 without saving, undoes window 1 back to its file from
 * the Edit menu, is told Build is still to come, wanders to Options and out
 * again, and quits from the File menu without saving window 2.
 * test/run.sh reads /ade.out, written as ade finishes, against what that has
 * to leave: two windows, each holding its file and what was typed into it --
 * a 2 in window 2 saying the second a.txt was refused rather than opened, and
 * a # saying that closing window 3 went back to window 2.
 */
#include "script.h"

#include <agon/mos.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "windows.h"

#define K(ch, vk)       { ch, vk, 0 }
#define M(ch, vk, mods) { ch, vk, mods }

static const key_press WINDOWS[] = {
    K('x', VK_x),                                   // a: xalpha
    K(0, VK_F3),                                    // open in a new window...
    K('b', VK_b), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // ...b.txt, window 2
    K('y', VK_y),                                   // b: ybravo
    K(0, VK_F3),                                    // /a.txt: a.txt again...
    K('/', VK_SLASH),
    K('a', VK_a), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // ...is window 1's
    K('m', VK_m),                                   // ...which the message waits on
    K('2', VK_2),                                   // b: y2bravo -- still window 2
    K(0, VK_F3),
    K('c', VK_c), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // c.txt, window 3
    K(0, VK_F3),                                    // a fourth: three are open
    K('k', VK_k),                                   // ...which the message waits on
    K(0, VK_F6),                                    // next: round to window 1
    K('z', VK_z),                                   // a: xzalpha, after the x
    M(0, VK_F6, MOD_ALT),                           // the list, at window 1
    K(0, VK_DOWN), K(0, VK_DOWN), K(13, VK_RETURN), // ...window 3
    K('!', VK_EXCLAIM),                         // c: !charlie
    M(0, VK_F3, MOD_ALT),                           // close window 3...
    K('n', VK_n),                                   // ...not saving; back to 2
    K('#', VK_HASH),                                // b: y#bravo, so it is 2
    K(0, VK_F6),                                    // next: round to window 1
    K(0, VK_F10), K(0, VK_RIGHT), K(13, VK_RETURN), // the menus: Edit, Undo --
                                                    // x and z were one run of
                                                    // typing, so a: alpha, as
                                                    // saved, and no longer
                                                    // unsaved
    K(0, VK_F10), K(0, VK_RIGHT), K(0, VK_RIGHT), K(0, VK_RIGHT),
    K(13, VK_RETURN),                               // Build: not yet...
    K('k', VK_k),                                   // ...which the message waits on
    K(0, VK_F10), K(0, VK_LEFT), K(27, VK_ESCAPE),  // round to Options, and out
    K(0, VK_F10),                                   // File...
    K(0, VK_DOWN), K(0, VK_DOWN), K(0, VK_DOWN), K(0, VK_DOWN),
    K(0, VK_DOWN), K(0, VK_DOWN), K(0, VK_DOWN),
    K(13, VK_RETURN),                               // ...Quit: only 2 is unsaved
    K('n', VK_n),                                   // ...leave it as it was
};
// Started on a source file: build it, F9. hub runs the tool, and ade again.
static const key_press BUILD[] = {
    K(0, VK_F9),
};

// Started on good.txt, with more for the build to carry across: good.txt
// changed in window 1, and good.c opened in window 2, changed with the cursor
// after it on line 3 -- all unsaved, and window 2 on screen at F9, which
// builds the file on screen.
static const key_press GOOD[] = {
    K(0, VK_END),                                   // good.txt: after "bravo"
    K('y', VK_y),                                   // bravoy
    K(0, VK_F3),                                    // open in a new window...
    K('g', VK_g), K('o', VK_o), K('o', VK_o), K('d', VK_d), K('.', VK_PERIOD),
    K('c', VK_c),
    K(13, VK_RETURN),                               // ...good.c, window 2
    K(0, VK_DOWN), K(0, VK_DOWN),                   // line 3
    K(' ', VK_SPACE),                               // "     return 0;"
    K(0, VK_F9),
};

// Started on wrong.c, in a project that cannot be built: F9 says why, which
// waits for a key, and ade stays -- so leaving is up to the script.
static const key_press WRONG[] = {
    K(0, VK_F9),
    K('k', VK_k),                                   // ...which the message waits on
    M(17, VK_q, MOD_CTRL),
};

// Started on a file whose name has "run" in it: build and run it, CTRL+F9.
static const key_press RUN[] = {
    M(0, VK_F9, MOD_CTRL),
};

// Started by hub after the build: what it says -- the error, or that it was
// built -- waits for a key; then, every window having been saved before the
// build, leaving asks nothing.
static const key_press RESUME[] = {
    K('k', VK_k),                                   // the error, or "Built"
    M(17, VK_q, MOD_CTRL),
};

// Resumed from a build that left three errors -- when /errlist is on the
// card, since every resume is started the same way. The list is up, on the
// first: to the third, in add.c, which opens in window 2; then the list again,
// which starts at the third, up to the second, back in main.c; and leave.
static const key_press ERRLIST[] = {
    K(0, VK_DOWN), K(0, VK_DOWN), K(13, VK_RETURN), // add.c:1:3
    K('k', VK_k),                                   // ...which the message waits on
    M(0, VK_F9, MOD_ALT),                           // the list again...
    K(0, VK_UP), K(13, VK_RETURN),                  // ...main.c:4:1
    K('k', VK_k),                                   // ...which the message waits on
    M(17, VK_q, MOD_CTRL),
};

#define COUNT(a) ((int) (sizeof(a) / sizeof(a[0])))

static const key_press* KEYS = WINDOWS;
static int NKEYS = COUNT(WINDOWS);

static int at;

// When main began, and when ade first asked for a key: ready, its screen
// drawn -- or, resumed, the build's outcome on it.
static clock_t began;
static clock_t ready;

void script_begin(void) {
    began = clock();
}

static bool poll(void* ctx, key_press* kp) {
    (void) ctx;
    if (at == 0 && ready == 0) {
        ready = clock();
    }
    if (at >= NKEYS) {
        return false;           // run out: ade waits, and the test times out
    }
    *kp = KEYS[at++];

    return true;
}

static const key_source SCRIPT = { poll, NULL, NULL };

const key_source* script_keys(const char* arg) {
    if (arg != NULL && strcmp(arg, "-resume") == 0) {
        const char fh = mos_fopen("/errlist", FA_READ);
        if (fh != 0) {
            mos_fclose(fh);
            KEYS = ERRLIST;
            NKEYS = COUNT(ERRLIST);
        } else {
            KEYS = RESUME;
            NKEYS = COUNT(RESUME);
        }
    } else if (arg != NULL && strstr(arg, "run") != NULL) {
        KEYS = RUN;
        NKEYS = COUNT(RUN);
    } else if (arg != NULL && strstr(arg, "wrong") != NULL) {
        KEYS = WRONG;
        NKEYS = COUNT(WRONG);
    } else if (arg != NULL && strstr(arg, "good") != NULL) {
        KEYS = GOOD;
        NKEYS = COUNT(GOOD);
    } else if (arg != NULL && (strstr(arg, ".c") != NULL || strstr(arg, ".s") != NULL)) {
        KEYS = BUILD;
        NKEYS = COUNT(BUILD);
    }

    return &SCRIPT;
}

static int line1(document* d, char* out, int max) {
    tb_seek(&d->buf_, (tb_pos) { .line = 1, .x = 0 });
    const split_line sl = tb_curr_line(&d->buf_);
    int n = sl.ssz_;
    while (n > 0 && (sl.suffix_[n - 1] == '\r' || sl.suffix_[n - 1] == '\n')) {
        n--;
    }
    if (n > max - 1) {
        n = max - 1;
    }
    memcpy(out, sl.suffix_, (size_t) n);
    out[n] = 0;

    return n;
}

void script_report(editor* ed) {
    static char out[512];
    static char text[64];
    int n = sprintf(out, "keys %d of %d\ncurrent %d\nat %d:%d\n", at, NKEYS,
                    win_current() + 1, tb_ypos(&ed->doc_->buf_),
                    tb_xpos(&ed->doc_->buf_));
    for (int i = 0; i < ADE_WINDOWS; i++) {
        document* d = win_doc(ed, i);
        if (d == NULL) {
            n += sprintf(out + n, "window %d closed\n", i + 1);
            continue;
        }
        const int y = tb_ypos(&d->buf_);
        const int x = tb_xpos(&d->buf_);
        line1(d, text, (int) sizeof(text));
        n += sprintf(out + n, "window %d %s %d:%d %s\n", i + 1, tb_fname(&d->buf_),
                     y, x, text);
    }
    const char fh = mos_fopen("/ade.out", FA_WRITE | FA_CREATE_ALWAYS);
    if (fh != 0) {
        mos_fwrite(fh, out, (unsigned) n);
        mos_fclose(fh);
    }

    // The start-up, in hundredths of a second: a start and a resume each
    // their own file, since a build's run has both.
    n = sprintf(out, "%lu\n", (unsigned long) ((ready - began) / (CLOCKS_PER_SEC / 100)));
    const char th = mos_fopen(KEYS == RESUME ? "/ade-resume.time" : "/ade.time",
                              FA_WRITE | FA_CREATE_ALWAYS);
    if (th != 0) {
        mos_fwrite(th, out, (unsigned) n);
        mos_fclose(th);
    }
}
