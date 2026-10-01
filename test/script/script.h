/*
 * A scripted run of ade, for the emulator: keys from a fixed sequence instead
 * of the keyboard, and a report of what ade ended with written to /ade.out.
 * Built only into the test binary test/run.sh makes, with -DADE_SCRIPT.
 */
#ifndef _SCRIPT_H_
#define _SCRIPT_H_

#include "editor.h"
#include "keys.h"

// The keys for a run started with `arg` -- its first argument, or NULL.
const key_source* script_keys(editor* ed, const char* arg);
// Called first thing in main: the start-up is timed from here.
void script_begin(void);
void script_report(editor* ed);

#endif  // _SCRIPT_H_
