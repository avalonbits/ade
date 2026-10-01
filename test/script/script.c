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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <hub/hub.h>

#include "menu.h"
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

// Started on tabs.txt, a tab and an x, with /config/ade.ini saying tab = 2:
// to the end of the line, which is drawn at column 4 with that tab and at 6
// with the default.
static const key_press TABS[] = {
    K(0, VK_END),
    M(17, VK_q, MOD_CTRL),
};

// Started on settings.txt, the same line, with no settings file yet. CTRL+E
// sets the tab to 8; then Options -> Settings, from the menu, sets it to 6;
// then to the end of the line again, drawn at column 8 with that tab.
static const key_press SETTINGS[] = {
    M(5, VK_e, MOD_CTRL),                           // the settings...
    K(13, VK_RETURN),                               // ...tab width, "4"
    K(8, VK_BACKSPACE), K('8', VK_8), K(13, VK_RETURN),
    K(27, VK_ESCAPE),                               // ...written: tab = 8
    K(0, VK_F10), K(0, VK_LEFT), K(13, VK_RETURN),  // Options, Settings...
    K(13, VK_RETURN),                               // ...tab width, "8"
    K(8, VK_BACKSPACE), K('6', VK_6), K(13, VK_RETURN),
    K(27, VK_ESCAPE),                               // ...written: tab = 6
    K(0, VK_HOME), K(0, VK_END),
    M(17, VK_q, MOD_CTRL),
};

// Started on memory.txt: m2.txt and m3.txt in the other two windows, all
// three too big for a window's memory, so each pages from the card -- ade at
// its fullest -- and leave. The report then says how much memory is left.
static const key_press MEMORY[] = {
    K(0, VK_F3),
    K('m', VK_m), K('2', VK_2), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    K(0, VK_F3),
    K('m', VK_m), K('3', VK_3), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(17, VK_q, MOD_CTRL),
};

// Started on pull.txt: the File menu pulled down, the second item chosen;
// with it up, the pixels of an item, the chosen one, the bar and the text
// are read back from the VDP; then out of it, and leave.
static const key_press PULL[] = {
    K(0, VK_F10),
    K(0, VK_DOWN),
    K(27, VK_ESCAPE),
    M(17, VK_q, MOD_CTRL),
};

// Started on one.txt: func.h opened in a new window, the screen read back as
// the next key is asked for; then F6, back to window 1, and the screen read
// back again; and leave.
static const key_press TWO[] = {
    K(0, VK_F3),
    K('f', VK_f), K('u', VK_u), K('n', VK_n), K('c', VK_c), K('.', VK_PERIOD),
    K('h', VK_h), K(13, VK_RETURN),
    K(0, VK_F6),
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

// Resumed after Run, when /userscreen is on the card: past what the program
// returned, then ALT+F5 -- the program's screen -- and a key back to ade.
// A pixel is read off the screen as each of the last two keys is asked for:
// with the program's screen up, and with ade's back.
static const key_press USERSCREEN[] = {
    K('k', VK_k),                                   // "... returned 0"
    M(0, VK_F5, MOD_ALT),                           // the program's screen...
    K('k', VK_k),                                   // ...and back
    M(17, VK_q, MOD_CTRL),
};

#define COUNT(a) ((int) (sizeof(a) / sizeof(a[0])))

static const key_press* KEYS = WINDOWS;
static int NKEYS = COUNT(WINDOWS);

static int at;

// The pixel at logical (300, 300) -- R, G, B -- as the VDP reports it, or
// 0xFFFFFF when it does not answer. Asked with VDU 23,0,&84; the answer lands
// in MOS's system variables with a flag that says it has.
#define PFLAGS      0x04
#define PFLAG_POINT 0x04
#define SCRPIXEL    0x0A

static unsigned long pixel_at(int x, int y) {
    volatile unsigned char* sv = mos_sysvars();
    const char ask[] = { 23, 0, (char) 0x84, (char) (x & 0xFF), (char) (x >> 8),
                         (char) (y & 0xFF), (char) (y >> 8) };
    sv[PFLAGS] &= (unsigned char) ~PFLAG_POINT;
    mos_puts((char*) ask, sizeof(ask), 0);
    for (long i = 0; i < 200000 && !(sv[PFLAGS] & PFLAG_POINT); i++) {
    }
    if (!(sv[PFLAGS] & PFLAG_POINT)) {
        return 0xFFFFFFUL;
    }

    return (unsigned long) sv[SCRPIXEL] << 16 | (unsigned long) sv[SCRPIXEL + 2] << 8
           | sv[SCRPIXEL + 1];
}

static unsigned long pixel(void) {
    return pixel_at(300, 300);
}

// The pixel in the middle of a character cell, in pixel coordinates for the
// moment it takes.
static unsigned long cell_pixel(int col, int row) {
    volatile unsigned char* sv = mos_sysvars();
    const int cols = sv[0x13];
    const int rows = sv[0x14];
    const int cw = cols > 0 ? (sv[0x0F] | sv[0x10] << 8) / cols : 8;
    const int ch = rows > 0 ? (sv[0x11] | sv[0x12] << 8) / rows : 8;
    static const char pixels[] = { 23, 0, (char) 0xC0, 0 };
    static const char logical[] = { 23, 0, (char) 0xC0, 1 };
    mos_puts((char*) pixels, sizeof(pixels), 0);
    const unsigned long p = pixel_at(col * cw + cw / 2, row * ch + ch / 2);
    mos_puts((char*) logical, sizeof(logical), 0);

    return p;
}

static char pulled[96];

static unsigned long seen[2];

// The text on the screen, read back from the VDP a character at a time with
// VDU 23,0,&83 -- what is really there, rather than what ade meant to put
// there -- one line per row into `path`, a cell it does not answer for as '?'.
#define SCRCHAR      0x09
#define PFLAG_CHAR   0x02
#define SCRCOLS      0x13
#define SCRROWS      0x14

// The editor, for its colours: see script_keys.
static editor* scripted;

// The character at (x, y), as the VDP matches it against the text colours
// in force: 0 when nothing matches, '?' when the VDP does not answer.
static char char_at(int x, int y) {
    volatile unsigned char* sv = mos_sysvars();
    const char ask[] = { 23, 0, (char) 0x83, (char) x, 0, (char) y, 0 };
    sv[PFLAGS] &= (unsigned char) ~PFLAG_CHAR;
    mos_puts((char*) ask, sizeof(ask), 0);
    long i = 0;
    while (i < 100000 && !(sv[PFLAGS] & PFLAG_CHAR)) {
        i++;
    }

    return (sv[PFLAGS] & PFLAG_CHAR) ? (char) sv[SCRCHAR] : '?';
}

static void dump_screen(const char* path) {
    volatile unsigned char* sv = mos_sysvars();
    const int cols = sv[SCRCOLS];
    const int rows = sv[SCRROWS];
    static char line[130];
    const char fh = mos_fopen(path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fh == 0) {
        return;
    }
    for (int y = 0; y < rows; y++) {
        int n = 0;
        for (int x = 0; x < cols && n < 128; x++) {
            char c = char_at(x, y);
            // The bars are in the document's colours swapped, and the VDP
            // matches a cell against the colours in force: asked again with
            // them swapped, a bar's text reads back too.
            if ((c == 0 || c == ' ') && scripted != NULL) {
                const char swap[] = { 17, scripted->scr_.bg_, 17,
                                      (char) (scripted->scr_.fg_ + 128) };
                const char back[] = { 17, scripted->scr_.fg_, 17,
                                      (char) (scripted->scr_.bg_ + 128) };
                mos_puts((char*) swap, sizeof(swap), 0);
                const char d = char_at(x, y);
                mos_puts((char*) back, sizeof(back), 0);
                if (d > ' ' && d < 127) {
                    c = d;
                }
            }
            line[n++] = c >= 32 && c < 127 ? c : (c == 0 ? ' ' : '?');
        }
        line[n++] = '\n';
        mos_fwrite(fh, line, (unsigned) n);
    }
    // And the file the footer last showed, as the screen recorded drawing it:
    // the footer's row does not read back, swapped colours or not.
    if (scripted != NULL) {
        const int k = sprintf(line, "footer %.100s\n", scripted->scr_.lastFname_);
        mos_fwrite(fh, line, (unsigned) k);
    }
    mos_fclose(fh);
}

// The screen mode, from MOS's system variables: as ade started, for the user
// screen's check that it comes back in it.
#define SCRMODE 0x27
static int start_mode;

// When main began, and when ade first asked for a key: ready, its screen
// drawn -- or, resumed, the build's outcome on it.
static clock_t began;
static clock_t ready;

void script_begin(void) {
    start_mode = mos_sysvars()[SCRMODE];
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
    // A resume's screen as its first key is asked for -- what the build said,
    // on top of the document -- and as its second is, once that is gone.
    if (KEYS == TWO && (at == 8 || at == 9)) {
        dump_screen(at == 8 ? "/ade.scr" : "/ade.scr2");
    }
    if (KEYS == RESUME && at < 2) {
        dump_screen(at == 0 ? "/ade.scr" : "/ade.scr2");
    }
    // The menu pulled down, as ESC is asked for: the blank cell just inside
    // its right border, on the first item's row and on the chosen second's;
    // the bar's last column; and the text area's, beside the box.
    if (KEYS == PULL && at == 2) {
        const view* b = menu_box();
        const int col = b->textX_ + b->cols_ - 2;
        const int last = mos_sysvars()[0x13] - 1;
        sprintf(pulled, "item %06lx chosen %06lx bar %06lx text %06lx\n",
                cell_pixel(col, b->topY_ + 1), cell_pixel(col, b->topY_ + 2),
                cell_pixel(last, 1), cell_pixel(last, b->topY_ + 1));
    }
    if (KEYS == USERSCREEN && (at == 2 || at == 3)) {
        seen[at - 2] = pixel();
    }
    *kp = KEYS[at++];

    return true;
}

static const key_source SCRIPT = { poll, NULL, NULL };

const key_source* script_keys(editor* ed, const char* arg) {
    scripted = ed;
    if (arg != NULL && strcmp(arg, "-resume") == 0) {
        const char fh = mos_fopen("/errlist", FA_READ);
        const char uh = fh == 0 ? mos_fopen("/userscreen", FA_READ) : 0;
        if (fh != 0) {
            mos_fclose(fh);
            KEYS = ERRLIST;
            NKEYS = COUNT(ERRLIST);
        } else if (uh != 0) {
            mos_fclose(uh);
            KEYS = USERSCREEN;
            NKEYS = COUNT(USERSCREEN);
        } else {
            KEYS = RESUME;
            NKEYS = COUNT(RESUME);
        }
    } else if (arg != NULL && strstr(arg, "one.txt") != NULL) {
        KEYS = TWO;
        NKEYS = COUNT(TWO);
    } else if (arg != NULL && strstr(arg, "pull") != NULL) {
        KEYS = PULL;
        NKEYS = COUNT(PULL);
    } else if (arg != NULL && strstr(arg, "memory") != NULL) {
        KEYS = MEMORY;
        NKEYS = COUNT(MEMORY);
    } else if (arg != NULL && strstr(arg, "tabs") != NULL) {
        KEYS = TABS;
        NKEYS = COUNT(TABS);
    } else if (arg != NULL && strstr(arg, "settings") != NULL) {
        KEYS = SETTINGS;
        NKEYS = COUNT(SETTINGS);
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

// How much memory is left, in KB: 1 KB blocks taken until there are no more,
// then given back. The heap and the stack grow toward each other out of one
// region, so this is what both have left between them.
static int free_kb(void) {
    static void* blocks[512];
    int n = 0;
    while (n < 512 && (blocks[n] = malloc(1024)) != NULL) {
        n++;
    }
    for (int i = 0; i < n; i++) {
        free(blocks[i]);
    }

    return n;
}

void script_report(editor* ed) {
    static char out[512];
    static char text[64];
    // Where the cursor is in the text, and where it is drawn: the row in the
    // view and the column along the line, both from 1. The two agree only if
    // what put the cursor somewhere also put the screen's cursor there.
    const view* v = ed->scr_.v_;
    int n = sprintf(out, "keys %d of %d\ncurrent %d\nat %d:%d\nscreen %d:%d\n", at,
                    NKEYS, win_current() + 1, tb_ypos(&ed->doc_->buf_),
                    tb_xpos(&ed->doc_->buf_), v->currY_ - v->topY_ + 1,
                    v->originX_ + v->currX_ + 1);
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

    // The header row as it reads now, to its last character.
    static char head[128];
    int hn = menu_bar_text(&ed->scr_, head, (int) sizeof(head) - 1);
    while (hn > 0 && head[hn - 1] == ' ') {
        hn--;
    }
    head[hn++] = '\n';
    const char hh = mos_fopen("/ade.head", FA_WRITE | FA_CREATE_ALWAYS);
    if (hh != 0) {
        mos_fwrite(hh, head, (unsigned) hn);
        mos_fclose(hh);
    }

    if (KEYS == USERSCREEN) {
        n = sprintf(out, "user %06lx ade %06lx mode %d %d captured %d\n", seen[0],
                    seen[1], start_mode, mos_sysvars()[SCRMODE], hub_user_screen());
        const char ph = mos_fopen("/ade.px", FA_WRITE | FA_CREATE_ALWAYS);
        if (ph != 0) {
            mos_fwrite(ph, out, (unsigned) n);
            mos_fclose(ph);
        }
    }

    // The colour of the menu bar's last column, of the text area's under it,
    // and of the footer's last column -- the screen's own pixels, in pixel
    // coordinates for the moment, the middle of each cell.
    {
        volatile unsigned char* sv = mos_sysvars();
        const int cols = sv[SCRCOLS];
        const int rows = sv[SCRROWS];
        const int cw = cols > 0 ? (sv[0x0F] | sv[0x10] << 8) / cols : 8;
        const int ch = rows > 0 ? (sv[0x11] | sv[0x12] << 8) / rows : 8;
        const int x = (cols - 1) * cw + cw / 2;
        static const char pixels[] = { 23, 0, (char) 0xC0, 0 };
        static const char logical[] = { 23, 0, (char) 0xC0, 1 };
        mos_puts((char*) pixels, sizeof(pixels), 0);
        const unsigned long bar = pixel_at(x, 1 * ch + ch / 2);
        const unsigned long text = pixel_at(x, 2 * ch + ch / 2);
        const unsigned long foot = pixel_at(x, (rows - 1) * ch + ch / 2);
        mos_puts((char*) logical, sizeof(logical), 0);
        n = sprintf(out, "bar %06lx text %06lx footer %06lx\n", bar, text, foot);
        const char bh = mos_fopen("/ade.bar", FA_WRITE | FA_CREATE_ALWAYS);
        if (bh != 0) {
            mos_fwrite(bh, out, (unsigned) n);
            mos_fclose(bh);
        }
    }

    if (KEYS == PULL) {
        const char ph = mos_fopen("/ade.pull", FA_WRITE | FA_CREATE_ALWAYS);
        if (ph != 0) {
            mos_fwrite(ph, pulled, (unsigned) strlen(pulled));
            mos_fclose(ph);
        }
    }

    if (KEYS == MEMORY) {
        n = sprintf(out, "%d\n", free_kb());
        const char mh = mos_fopen("/ade.mem", FA_WRITE | FA_CREATE_ALWAYS);
        if (mh != 0) {
            mos_fwrite(mh, out, (unsigned) n);
            mos_fclose(mh);
        }
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
