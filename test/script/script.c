/*
 * The keys a person would press to use ade's windows, played on real MOS.
 *
 * The card holds a.txt ("alpha"), b.txt ("bravo") and c.txt ("charlie"), and
 * ade starts on a.txt. The run types in each window, opens the other two with
 * F3, is told that /a.txt is window 1's a.txt already and at the fourth that three
 * are open, moves with F6 and the ALT+F6
 * list, closes window 3 without saving, and quits without saving the others.
 * test/run.sh reads /ade.out, written as ade finishes, against what that has
 * to leave: two windows, each holding its file and what was typed into it --
 * a 2 in window 2 saying the second a.txt was refused rather than opened, and
 * a # saying that closing window 3 went back to window 2.
 */
#include "script.h"

#include <agon/mos.h>
#include <stdio.h>
#include <string.h>

#include "windows.h"

#define K(ch, vk)       { ch, vk, 0 }
#define M(ch, vk, mods) { ch, vk, mods }

static const key_press KEYS[] = {
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
    M(17, VK_q, MOD_CTRL),                          // quit: 1 and 2 are unsaved
    K('n', VK_n), K('n', VK_n),                     // ...leave both as they were
};
#define NKEYS ((int) (sizeof(KEYS) / sizeof(KEYS[0])))

static int at;

static bool poll(void* ctx, key_press* kp) {
    (void) ctx;
    if (at >= NKEYS) {
        return false;           // run out: ade waits, and the test times out
    }
    *kp = KEYS[at++];

    return true;
}

static const key_source SCRIPT = { poll, NULL, NULL };

const key_source* script_keys(void) {
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
    int n = sprintf(out, "keys %d of %d\ncurrent %d\n", at, NKEYS, win_current() + 1);
    for (int i = 0; i < ADE_WINDOWS; i++) {
        document* d = win_doc(ed, i);
        if (d == NULL) {
            n += sprintf(out + n, "window %d closed\n", i + 1);
            continue;
        }
        line1(d, text, (int) sizeof(text));
        n += sprintf(out + n, "window %d %s %s\n", i + 1, tb_fname(&d->buf_), text);
    }
    const char fh = mos_fopen("/ade.out", FA_WRITE | FA_CREATE_ALWAYS);
    if (fh != 0) {
        mos_fwrite(fh, out, (unsigned) n);
        mos_fclose(fh);
    }
}
