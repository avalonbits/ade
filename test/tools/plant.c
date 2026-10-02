/*
 * Leaves a session in hub's block, as ade does before a build, naming
 * planted.txt in window 1 -- for test/run.sh, which then starts ade -resume.
 * `plant other` writes it as an ade with another layout would: the same
 * MAGIC, another size. Built with agondev, against ade's session.h.
 */
#include <agon/mos.h>
#include <string.h>

#include <hub/hub.h>

#include "session.h"

#define SYSVAR_SCRMODE 0x27

int main(int argc, char** argv) {
    if (!hub_present()) {
        return 1;
    }
    session* s = (session*) hub_block(SESSION_TAG, sizeof(session));
    if (s == NULL) {
        return 1;
    }
    memset(s, 0, sizeof(session));
    memcpy(s->magic, MAGIC, 4);
    s->size = sizeof(session);
    if (argc > 1 && strcmp(argv[1], "other") == 0) {
        s->size--;
    }
    s->mode = ((volatile unsigned char*) mos_sysvars())[SYSVAR_SCRMODE];
    s->open[0] = 1;
    strcpy(s->name[0], "planted.txt");

    return 0;
}
