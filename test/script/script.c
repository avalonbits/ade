/*
 * The keys a person would press, played on real MOS: which keys depends on how
 * ade was started. On a.txt, the windows and the menus, below. On a source
 * file, F9, to build it; and when hub brings ade back, -resume, leaving.
 *
 * The windows and the menus:
 *
 * The card holds a.txt ("alpha"), b.txt ("bravo") and c.txt ("charlie"), and
 * ade starts on a.txt. The run types in each window, opens the other two
 * with CTRL+SHIFT+O, is told that /a.txt is window 1's a.txt already, moves
 * with CTRL+TAB and the ALT+F6
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

#include "build.h"
#include "menu.h"
#include "windows.h"

#define K(ch, vk)       { ch, vk, 0 }
#define M(ch, vk, mods) { ch, vk, mods }

static const key_press WINDOWS[] = {
    K('x', VK_x),                                   // a: xalpha
    M(15, VK_O, MOD_CTRL | MOD_SHFT),                                    // open in a new window...
    K('b', VK_b), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // ...b.txt, window 2
    K('y', VK_y),                                   // b: ybravo
    M(15, VK_O, MOD_CTRL | MOD_SHFT),                                    // /a.txt: a.txt again...
    K('/', VK_SLASH),
    K('a', VK_a), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // ...is window 1's
    K('m', VK_m),                                   // ...which the message waits on
    K('2', VK_2),                                   // b: y2bravo -- still window 2
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('c', VK_c), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // c.txt, window 3
    M(9, VK_TAB, MOD_CTRL),                                    // next: round to window 1
    K('z', VK_z),                                   // a: xzalpha, after the x
    M(0, VK_F6, MOD_ALT),                           // the list, at window 1
    K(0, VK_DOWN), K(0, VK_DOWN), K(13, VK_RETURN), // ...window 3
    K('!', VK_EXCLAIM),                         // c: !charlie
    M(0, VK_F3, MOD_ALT),                           // close window 3...
    K('n', VK_n),                                   // ...not saving; back to 2
    K('#', VK_HASH),                                // b: y#bravo, so it is 2
    M(9, VK_TAB, MOD_CTRL),                                    // next: round to window 1
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
    K(0, VK_DOWN), K(0, VK_DOWN), K(0, VK_DOWN), K(0, VK_DOWN),
    K(0, VK_DOWN),                                  // past Shell...
    K(13, VK_RETURN),                               // ...Quit: only 2 is unsaved
    K('n', VK_n),                                   // ...leave it as it was
};
// Started on a source file: build it, F9. hub runs the tool, and ade again.
static const key_press BUILD[] = {
    M(2, VK_b, MOD_CTRL),                           // CTRL+B
};

// Started on good.txt, with more for the build to carry across: good.txt
// changed in window 1, and good.c opened in window 2, changed with the cursor
// after it on line 3 -- all unsaved, and window 2 on screen at F9, which
// builds the file on screen.
static const key_press GOOD[] = {
    K(0, VK_END),                                   // good.txt: after "bravo"
    K('y', VK_y),                                   // bravoy
    M(15, VK_O, MOD_CTRL | MOD_SHFT),                                    // open in a new window...
    K('g', VK_g), K('o', VK_o), K('o', VK_o), K('d', VK_d), K('.', VK_PERIOD),
    K('c', VK_c),
    K(13, VK_RETURN),                               // ...good.c, window 2
    K(0, VK_DOWN), K(0, VK_DOWN),                   // line 3
    K(' ', VK_SPACE),                               // "     return 0;"
    M(2, VK_b, MOD_CTRL),
};

// Started on wrong.c, in a project that cannot be built: F9 says why, which
// waits for a key, and ade stays -- so leaving is up to the script.
static const key_press WRONG[] = {
    M(2, VK_b, MOD_CTRL),
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
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('m', VK_m), K('2', VK_2), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
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
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('u', VK_u), K('n', VK_n), K('c', VK_c), K('.', VK_PERIOD),
    K('h', VK_h), K(13, VK_RETURN),
    M(9, VK_TAB, MOD_CTRL),
    M(17, VK_q, MOD_CTRL),
};

// Started on newproj.c: Build -> New project, the name it offers taken, what
// it says waited past, and F9. Where there is a project.ini already, what it
// says instead is that, which the same key waits past.
static const key_press NEWPROJ[] = {
    K(0, VK_F10), K(0, VK_RIGHT), K(0, VK_RIGHT), K(0, VK_RIGHT),  // Build...
    K(0, VK_DOWN), K(0, VK_DOWN), K(13, VK_RETURN),                // ...New project
    K(13, VK_RETURN),                               // the name offered
    K('k', VK_k),                                   // ...which the message waits on
    M(2, VK_b, MOD_CTRL),
};

// Started on close1.txt: b.txt and c.txt in windows 2 and 3, then F6 round
// to window 1 and ALT+F3 to close it -- b.txt is window 1 then, c.txt window
// 2, and the screen is read back as the next key is asked for -- then F6 to
// c.txt and closed too, back to b.txt; closing that, the last, is refused,
// which the message waits on; and leave.
static const key_press CLOSE[] = {
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('b', VK_b), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // b.txt, window 2
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('c', VK_c), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // c.txt, window 3
    M(9, VK_TAB, MOD_CTRL),                                    // round to window 1
    M(0, VK_F3, MOD_ALT),                           // ...closed: b.txt is 1
    M(9, VK_TAB, MOD_CTRL),                                    // c.txt, window 2
    M(0, VK_F3, MOD_ALT),                           // ...closed: back to b.txt
    M(0, VK_F3, MOD_ALT),                           // the last: refused...
    K('k', VK_k),                                   // ...which the message waits on
    M(17, VK_q, MOD_CTRL),
};

// Started on nav.txt: b.txt and c.txt in windows 2 and 3, then round the
// windows by the keys for it -- the window on screen noted as each of those
// keys is asked for: CTRL+TAB, CTRL+SHIFT+TAB twice, CTRL+1, CTRL+3, ALT+F3
// closing window 3, CTRL+3 with no window 3 any more, and F6 and F9, which
// were next window and build and are nothing now; and leave.
static const key_press NAV[] = {
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('b', VK_b), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('c', VK_c), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x), K('t', VK_t),
    K(13, VK_RETURN),                               // 3 on screen
    M(9, VK_TAB, MOD_CTRL),                         // round to 1
    M(9, VK_TAB, MOD_CTRL | MOD_SHFT),              // back round to 3
    M(9, VK_TAB, MOD_CTRL | MOD_SHFT),              // 2
    M('1', VK_1, MOD_CTRL),                         // 1
    M('3', VK_3, MOD_CTRL),                         // 3
    M(0, VK_F3, MOD_ALT),                           // closed: 2
    M('3', VK_3, MOD_CTRL),                         // no window 3: still 2
    K(0, VK_F6),                                    // F6 and F9 are nothing
    K(0, VK_F9),                                    // now: still 2, and 2
    M(17, VK_q, MOD_CTRL),
};

// Which window was on screen as each of NAV's last keys was asked for.
static char nav_seen[16];
static int nav_n;

// Started on w1.txt: four windows, three of them loaded at a time. Each of
// w1, w2 and w3 changed, unsaved; then w4.txt, for which one must be put away
// and none can as it is -- so the one used longest ago, w1.txt, is to be
// saved first: no, and nothing opens; again, yes, and it is saved and put
// away. Then CTRL+1: w1.txt loaded back, putting away w4.txt, the one saved;
// CTRL+9, no window 9; and leaving, not saving w2 and w3.
static const key_press MANY[] = {
    K('A', VK_A),                                   // w1: Aone
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('w', VK_w), K('2', VK_2), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    K('B', VK_B),                                   // w2: Btwo
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('w', VK_w), K('3', VK_3), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    K('C', VK_C),                                   // w3: Cthree
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('w', VK_w), K('4', VK_4), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    K('n', VK_n),                                   // save w1.txt? no: no w4
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('w', VK_w), K('4', VK_4), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    K('y', VK_y),                                   // yes: w4.txt, window 4
    M('1', VK_1, MOD_CTRL),                         // w1.txt back
    M('9', VK_9, MOD_CTRL),                         // no window 9
    M(17, VK_q, MOD_CTRL),
    K('n', VK_n), K('n', VK_n),                     // w2 and w3 left unsaved
};

// Started on a file whose path is too long for a window to keep put away:
// w2.txt, w3.txt and w4.txt opened beside it, none changed. Making room for
// w4.txt, the window used longest ago is window 1 -- which cannot be put
// away, so w2.txt is; and leave.
static const key_press LONGNAME[] = {
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('w', VK_w), K('2', VK_2), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('w', VK_w), K('3', VK_3), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('w', VK_w), K('4', VK_4), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(17, VK_q, MOD_CTRL),
};

// Started on f1.txt: f2.txt to f9.txt opened beside it, nine windows, the
// earlier ones put away in turn as the later are loaded; then a tenth asked
// for, which is refused, which the message waits on; and leave.
static const key_press NINE[] = {
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('2', VK_2), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('3', VK_3), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('4', VK_4), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('5', VK_5), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('6', VK_6), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('7', VK_7), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('8', VK_8), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('f', VK_f), K('9', VK_9), K('.', VK_PERIOD), K('t', VK_t), K('x', VK_x),
    K('t', VK_t), K(13, VK_RETURN),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),               // a tenth: refused...
    K('k', VK_k),                                   // ...which the message waits on
    M(17, VK_q, MOD_CTRL),
};

// Started on a file whose name has "run" in it: build and run it, CTRL+F9.
static const key_press RUN[] = {
    M(18, VK_r, MOD_CTRL),                          // CTRL+R
};

// Started by hub after the build: an error waits for a key, which ESC is,
// and a build that worked waits for nothing, which ESC also does nothing to;
// then, every window having been saved before the build, leaving asks
// nothing.
static const key_press RESUME[] = {
    K(27, VK_ESCAPE),                               // past an error; else nothing
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
    K(27, VK_ESCAPE),                               // nothing: nothing waits
    M(0, VK_F5, MOD_ALT),                           // the program's screen...
    K('k', VK_k),                                   // ...and back
    M(17, VK_q, MOD_CTRL),
};

// Resumed after a build that worked, when /nowait is on the card: nothing
// to get past, so straight to leaving. A resume that waited on a message
// would take this key for it, and ade would not leave.
static const key_press NOWAIT[] = {
    M(17, VK_q, MOD_CTRL),
};

// shell.txt: a change, then CTRL+F12 to hub's shell -- which saves it first --
// and ade leaves. The script's next lines are typed at the shell, ending with
// exit, and ade comes back as a resume.
static const key_press SHELL[] = {
    K('x', VK_x),
    M(0, VK_F12, MOD_CTRL),
};

// Started with no file, when /banner is on the card: the banner is up as the
// first key is asked for, and that key leaves.
static const key_press BANNER_KEYS[] = {
    M(17, VK_q, MOD_CTRL),
};

// viewer.txt, started in /proj: the file viewer. Into src/ and open inner.c
// in a new window; narrow to beta.c by typing; CTRL+O, a new fresh.c over it;
// a directory with F7 and one from its row; and ../x.c, outside, refused.
static const key_press VIEWER[] = {
    M(15, VK_O, MOD_CTRL | MOD_SHFT),               // the viewer, on src/
    K(13, VK_RETURN),                               // into src/...
    K(13, VK_RETURN),                               // ...inner.c, window 2
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('b', VK_b), K('e', VK_e), K('t', VK_t),       // beta.c alone...
    K(13, VK_RETURN),                               // ...window 3
    M(15, VK_o, MOD_CTRL),                          // here, in 3:
    K('f', VK_f), K('r', VK_r), K('e', VK_e), K('s', VK_s), K('h', VK_h),
    K('.', VK_PERIOD), K('c', VK_c),
    K(13, VK_RETURN),                               // no such file: a new one
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K(0, VK_F7),                                    // a new directory...
    K('m', VK_m), K('a', VK_a), K('d', VK_d), K('e', VK_e),
    K(13, VK_RETURN),                               // ...made
    K(27, VK_ESCAPE),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K(0, VK_UP), K(13, VK_RETURN),                  // [ New directory ]...
    K('n', VK_n), K('e', VK_e), K('w', VK_w),
    K(13, VK_RETURN),                               // ...new
    K(27, VK_ESCAPE),
    M(15, VK_O, MOD_CTRL | MOD_SHFT),
    K('.', VK_PERIOD), K('.', VK_PERIOD), K('/', VK_SLASH),
    K('x', VK_x), K('.', VK_PERIOD), K('c', VK_c),
    K(13, VK_RETURN),                               // outside: refused...
    K('k', VK_k),                                   // ...which the message waits on
    K(27, VK_ESCAPE),
    M(17, VK_q, MOD_CTRL),
};

// named.c: CTRL+SHIFT+N, an x after the name offered, and RETURN. With no
// project here, a project named namedx; with namedx here, renamed namedxx.
static const key_press NAMED[] = {
    M(14, VK_N, MOD_CTRL | MOD_SHFT),
    K('x', VK_x),
    K(13, VK_RETURN),
    K('k', VK_k),                                   // ...which the message waits on
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
    // The screen's size as the editor laid it out, which it read once: MOS's
    // copy is the VDP's answer to the last mode question asked, and can be a
    // different one's by the time this asks.
    const int cols = scripted != NULL ? scripted->scr_.barW_ + 1 : sv[SCRCOLS];
    const int rows = scripted != NULL ? scripted->scr_.rows_ : sv[SCRROWS];
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
        // And the title the header row is drawn with, whose letters do not
        // read back either.
        const int t = sprintf(line, "title %.100s\n",
                              scripted->scr_.title_ != NULL ? scripted->scr_.title_ : "");
        mos_fwrite(fh, line, (unsigned) t);
    }
    // And the last thing a build said on the message row, which is the same.
    const int k = sprintf(line, "said %.100s\n", build_said());
    mos_fwrite(fh, line, (unsigned) k);
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
    if (KEYS == NAV && at >= 14 && nav_n < (int) sizeof(nav_seen) - 1) {
        nav_seen[nav_n++] = (char) ('1' + win_current());
    }
    if (KEYS == CLOSE && at == 16) {
        dump_screen("/ade.scr");
    }
    if (KEYS == TWO && (at == 8 || at == 9)) {
        dump_screen(at == 8 ? "/ade.scr" : "/ade.scr2");
    }
    // The viewer at the top, and in src/, as each is up.
    if (KEYS == VIEWER && (at == 1 || at == 2)) {
        dump_screen(at == 1 ? "/ade.scr" : "/ade.scr2");
    }
    if (KEYS == BANNER_KEYS && at == 0) {
        dump_screen("/ade.scr");
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
    const char bh = arg == NULL ? mos_fopen("/banner", FA_READ) : 0;
    if (bh != 0) {
        mos_fclose(bh);
        KEYS = BANNER_KEYS;
        NKEYS = COUNT(BANNER_KEYS);
    } else if (arg != NULL && strcmp(arg, "-resume") == 0) {
        const char fh = mos_fopen("/errlist", FA_READ);
        const char uh = fh == 0 ? mos_fopen("/userscreen", FA_READ) : 0;
        const char nw = fh == 0 && uh == 0 ? mos_fopen("/nowait", FA_READ) : 0;
        if (fh != 0) {
            mos_fclose(fh);
            KEYS = ERRLIST;
            NKEYS = COUNT(ERRLIST);
        } else if (uh != 0) {
            mos_fclose(uh);
            KEYS = USERSCREEN;
            NKEYS = COUNT(USERSCREEN);
        } else if (nw != 0) {
            mos_fclose(nw);
            KEYS = NOWAIT;
            NKEYS = COUNT(NOWAIT);
        } else {
            KEYS = RESUME;
            NKEYS = COUNT(RESUME);
        }
    } else if (arg != NULL && strstr(arg, "named.c") != NULL) {
        KEYS = NAMED;
        NKEYS = COUNT(NAMED);
    } else if (arg != NULL && strstr(arg, "viewer.txt") != NULL) {
        KEYS = VIEWER;
        NKEYS = COUNT(VIEWER);
    } else if (arg != NULL && strstr(arg, "shell.txt") != NULL) {
        KEYS = SHELL;
        NKEYS = COUNT(SHELL);
    } else if (arg != NULL && strstr(arg, "a_directory_with") != NULL) {
        KEYS = LONGNAME;
        NKEYS = COUNT(LONGNAME);
    } else if (arg != NULL && strstr(arg, "f1.txt") != NULL) {
        KEYS = NINE;
        NKEYS = COUNT(NINE);
    } else if (arg != NULL && strstr(arg, "w1.txt") != NULL) {
        KEYS = MANY;
        NKEYS = COUNT(MANY);
    } else if (arg != NULL && strstr(arg, "nav.txt") != NULL) {
        KEYS = NAV;
        NKEYS = COUNT(NAV);
    } else if (arg != NULL && strstr(arg, "close1") != NULL) {
        KEYS = CLOSE;
        NKEYS = COUNT(CLOSE);
    } else if (arg != NULL && strstr(arg, "one.txt") != NULL) {
        KEYS = TWO;
        NKEYS = COUNT(TWO);
    } else if (arg != NULL && strstr(arg, "pull") != NULL) {
        KEYS = PULL;
        NKEYS = COUNT(PULL);
    } else if (arg != NULL && strstr(arg, "memory") != NULL) {
        KEYS = MEMORY;
        NKEYS = COUNT(MEMORY);
    } else if (arg != NULL && strstr(arg, "newproj") != NULL) {
        KEYS = NEWPROJ;
        NKEYS = COUNT(NEWPROJ);
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
    static char out[1024];
    static char text[64];
    // Where the cursor is in the text, and where it is drawn: the row in the
    // view and the column along the line, both from 1. The two agree only if
    // what put the cursor somewhere also put the screen's cursor there.
    const view* v = ed->scr_.v_;
    int n = sprintf(out, "keys %d of %d\ncurrent %d\nat %d:%d\nscreen %d:%d\n", at,
                    NKEYS, win_current() + 1, tb_ypos(&ed->doc_->buf_),
                    tb_xpos(&ed->doc_->buf_), v->currY_ - v->topY_ + 1,
                    v->originX_ + v->currX_ + 1);
    // Three windows at least, as before there were more, and any past them.
    const int shown = win_count() > 3 ? win_count() : 3;
    for (int i = 0; i < shown; i++) {
        document* d = win_doc(ed, i);
        if (d == NULL && win_name(ed, i) != NULL) {
            int wl = 0;
            int wx = 0;
            win_where(ed, i, &wl, &wx);
            n += sprintf(out + n, "window %d %s %d:%d (put away)\n", i + 1,
                         win_name(ed, i), wl, wx + 1);
            continue;
        }
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
        n = sprintf(out, "settled %s\n", build_modes_settled());
        const char mh = mos_fopen("/ade.mode", FA_WRITE | FA_CREATE_ALWAYS);
        if (mh != 0) {
            mos_fwrite(mh, out, (unsigned) n);
            mos_fclose(mh);
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

    if (KEYS == NAV) {
        const char vh = mos_fopen("/ade.nav", FA_WRITE | FA_CREATE_ALWAYS);
        if (vh != 0) {
            mos_fwrite(vh, nav_seen, (unsigned) nav_n);
            mos_fclose(vh);
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
