/*
 * A scripted run of ade, for the emulator: keys from a fixed sequence instead
 * of the keyboard, and a report of what ade ended with written to /ade.out.
 * Built only into the test binary test/run.sh makes, with -DADE_SCRIPT.
 */
#ifndef _SCRIPT_H_
#define _SCRIPT_H_

#include "editor.h"
#include "keys.h"

const key_source* script_keys(void);
void script_report(editor* ed);

#endif  // _SCRIPT_H_
