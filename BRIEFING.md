# ade: the Agon Development Environment

This is the briefing for building **ade**. Read all of it before writing code.
It records the decisions already made, what has been measured in AED's source,
the plan, and what is still open.

## What ade is -- and what aed stays

- **aed** stays what it is: a simple, fast text editor for the Agon
  (`~/code/aed`, repository `avalonbits/aed`). It does not learn to compile,
  run programs or talk to hub. The user was explicit about this.
- **ade** is a new program, a development environment in the spirit of Turbo
  C/Turbo Pascal, built on **AED's core**: the same document model, paging,
  undo, find, syntax highlighting and themes, and much of the same view. On top
  of that it adds what an IDE needs: menus, several open files, building with
  acc and zap, a list of errors you can jump to (`file:line:col`), and running
  the program.
- **The core becomes a library** that both programs link. Refactoring AED so
  that this is possible is the first half of the work, and it happens in
  AED's repository, behaviour-preserving, one PR at a time.

The name follows the user's three-letter convention: aed, zap, acc, hub, pak,
dig -- and now ade.

## The neighbouring projects ade depends on

| Project | Where | What ade uses |
|---|---|---|
| **aed** | `~/code/aed` (read `AGENTS.md` and `.internal/docs/` first) | the core library and most of the view |
| **hub** -- resident shell/scheduler | `~/code/devel`; design: https://claude.ai/code/artifact/2284d613-0e89-40fc-bf85-5e81dd591918 | runs acc/zap/the program for ade and brings ade back; keeps ade's session in a hub block; `include/hub.h` + `libhub.a` |
| **zap** -- assembler | `~/code/zap`; `-e <file>` on branch `error-file`, PR avalonbits/zap#163 | assembling; its error file |
| **acc** -- C compiler | `~/code/acc` (another agent works there; read only); `-errors <file>` with columns on branch `error-file`, worktree `~/code/acc-diag` | compiling; its error file |
| **pak** -- library packaging (designed, not built) | https://claude.ai/code/artifact/ddf7e7fe-e8eb-4e6e-b4a4-6c79aae6962a | how ade gets `libedcore`/`libedui`; built before phase B |
| **dig** -- debugger (briefing only) | `~/code/dig/BRIEFING.md` | a later integration: breakpoints in ade, stop and come back |

Both tools write errors as `file:line:column: error: text`, one per line
(column 0 = unknown, a tab counts as one column), and fail with exit code 100,
which MOS passes through unchanged. AED keeps real tabs in its buffer and
expands them only on screen, so a tool's column maps straight to a buffer
column.

## What AED looks like today (measured)

AED is ~17k lines of C in `src/`, built with agondev (`Makefile` includes
agondev's `makefile.inc`). `bin/aed.bin` is ~92 KB: `.text` 87.6 KB, `.bss`
57 KB; heap and stack share the remaining ~310 KB below `0xB0000`. It supports
MOS 2.3.3 / VDP 1.04 and up; every change is checked on both the oldest and the
newest firmware.

Its own design notes are in `.internal/docs/`: `ARCHITECTURE.md` (the module
map, the document model, rendering, input), `MVC-REVIEW.md` (how the layers
hold up -- read it), `PAGING.md`, `SIZING.md` (including "what a second
document would cost"), `UNDO.md`, `SYNTAX.md`, `KEYBOARD.md`, `FONTS.md`,
`WALKER.md`.

### The core is already nearly a library

No core source includes a UI header. `MVC-REVIEW.md` measured the
Model/View boundary as clean in both directions, and its fixes for the
Controller/View leaks have landed (PRs #55, #57, #59, real tab support).

| Module | Role | Platform | State that stands in the way |
|---|---|---|---|
| `char_buffer` | a gap buffer (also used by prompts) | none | none |
| `line_buffer` | the line index, a gap buffer of ints | none | none |
| `text_buffer*` (6 files) | the document: edit, move, find, ranges, load/save, paging | MOS file calls, in `text_buffer_io.c` only | static scratch buffers (`text_buffer_page.c:82-90` slide/evict ~10 KB; `find_pass fp`; io's `buf`/`tmp`/`FILINFO`) -- fine one at a time, not re-entrant |
| `doc_store` | paging scratch files `<doc>.aedh`/`.aedt` | MOS files | falls back to the name `aed.aedh`/`aed.aedt` (doc_store.c:83-89) |
| `undo` | per-document operation log | none | none (16 KB + 256 records, sized in `undo.h`) |
| `clipboard` | 8 KB, spills to `<doc>.scratch` | MOS files | assumes one clipboard (clipboard.c:213-217); falls back to `aed.scratch` |
| `lexer` + `syntax.h`, `theme` | highlighting, grammars and themes from INI files | MOS reads | take paths; directories are chosen in `editor.c` |
| `ini` | INI reading | none | none -- fully generic |
| `config` | `/config/aed.ini` | MOS files | AED's path, schema (`SETTINGS[]`) and header text are baked in |
| `conv`, `bootfont` | `i2s`; the boot font from autoexec | -- | these are view support, not core |

### The front end is where one-editor assumptions live

- **The `editor` struct** (`editor.h:32-109`) holds everything: the document,
  undo, syntax state, selection, prompt, clipboard, find. The *view* (cursor
  row/column, horizontal scroll, text rectangle) lives in **`screen`**
  (`currX_`, `currY_`, `topY_`, `bottomY_`, `originX_`, `textX_`, `cols_`);
  the top document line isn't stored but derived.
- **Commands** (`cmd_ops.c`, `void cmd_x(editor*)`) edit the buffer *and*
  repaint through `scr_*` in the same function, each choosing the smallest VDP
  update -- a deliberate optimisation for the serial link to the VDP. Keep it.
- **Dispatch** is two `switch` statements (`ctrlCmds`, `editCmds` in
  `editor.c`) plus three sentinel "commands" (`CMD_PUTC/QUIT/SAVE`, fake
  pointers 0x01-0x03). Which commands own the selection is a hard-coded list
  of function pointers (`owns_selection`).
- **The loop** (`ed_run`, editor.c:579-653): footer, blocking `read_input`,
  selection, dispatch, `tb_settle` (mandatory before repaint), selection
  repaint.
- **Modal widgets** (`user_input.c`) each run their own blocking key loop --
  a documented, justified pattern. The list helpers (`modal_fill`,
  `modal_move`, `ui_font_picker`) are a base for ade's menus and error list.
  The help text, banner and settings are AED's.
- **Lifecycle**: `ed_init` does screen, config (`/config/aed.ini`),
  clipboard, document, colourer, syntax (`/config/aed/syntax`,
  `/config/aed/themes`), undo, prompt, and installs the key hook last;
  `ed_destroy` removes it first.

### Size and memory

| Part | Code |
|---|---|
| core (buffers, paging, find, undo, clipboard, lexer, theme, config, ini) | ~50 KB, plus ~57 KB bss |
| front end (cmd_ops 15.5 KB, screen 10.3, user_input 6.9, editor 4.2) | ~37 KB |

A document at `AED_DOC_KB` = 72 costs ~79 KB of heap, plus ~20 KB of undo:
~100 KB each. **SIZING.md §7**: two documents with two undo logs and a
clipboard leave ~119 KB free; three fit. **File handles run out before
memory**: MOS gives 7, a paged document holds 2 while open, so three paged
documents is the limit. For ade (core + view + ~20-30 KB of its own UI) the
estimate is ~160-175 KB of image and bss, leaving ~290 KB of heap: two
documents at 72 KB with room for an error list, or three at 48 KB (the tested
floor is 48).

## The target shape

Three layers, two programs:

```
   aed (main, keymap, help, settings, header/footer)     ade (menus, documents, build, errors, hub)
                     \                                        /
                      +------------- libedui ------------------+
                      |  view (a rectangle + cursor + scroll), |
                      |  screen device, widgets, key source,   |
                      |  editing commands, key translation     |
                      +----------------------------------------+
                                        |
                      +------------ libedcore -----------------+
                      |  document = buffer + undo + syntax     |
                      |  state; paging, find, clipboard,       |
                      |  lexer, theme, ini, config engine      |
                      |  -- no VDP, no keyboard                |
                      +----------------------------------------+
```

### libedcore -- the model, no VDP

- **A `document`** in core: `text_buffer` + `undo` + `syntax`/`theme` + the
  syntax-state cache (`lineSyn_`, `synFirst_`, `synKnown_`) + the selection
  anchor. Today these are loose fields of `editor`; moving them together is
  what makes "one of these per open file" possible.
- **An application context passed in, never baked in**: the app name (for
  scratch-file fallbacks, today the literal `"aed"`), the config directory,
  the syntax and theme directories. aed passes `aed`, `/config/aed/...`; ade
  passes `ade` for its name but the same `/config/aed/syntax` and
  `/config/aed/themes` (grammars and themes are shared).
- **The config engine made schema-driven**: `ini` + a settings table and field
  pointers passed in. AED's `SETTINGS[]` and `/config/aed.ini` move to the aed
  front end; ade brings its own table.
- **Constants out of UI headers**: `AED_DOC_KB` lives in `editor.h` today and
  is really core policy; tests include `editor.h` just to get it.
- **Static scratch buffers**: keep them -- they exist to keep stack frames
  under the eZ80's 128-byte `(IX+d)` reach -- but document that the core is
  not re-entrant across documents (a `tb_sink` callback on one document must
  not call into another). Only fix what ade actually hits.
- **What leaves core**: `bootfont` and `conv` belong with the screen.

### libedui -- the view and the editing machinery, shared

- **A `view`**: the text rectangle (x, y, width, height), cursor row and
  column, horizontal origin, `synTop_`, bound to one `document`. The fields
  move out of `screen`, which becomes the device: colours, font, character
  size, the output buffer, entry/exit state. With that, ade can show a
  document in any rectangle and keep a view per open file.
- **Commands take a view** (and through it the document), keeping their
  minimal-repaint logic. Anything that assumes `bottomY_ - 1` is the last text
  row or that the bar is on `bottomY_` goes through the view's rectangle.
- **An embeddable loop**: `ed_translate(key) -> key_command` (today's
  `read_input` without the blocking wait) and `ed_handle(view, key_command)`
  (dispatch, `tb_settle`, selection repaint). aed's `ed_run` becomes a short
  loop in aed's front end; ade runs its own loop and calls the same two.
- **A data-driven keymap with flags** (`KC_OWNS_SEL`, `KC_EDITS`, ...) instead
  of the pointer list and the sentinels, so a front end adds its own keys (F9,
  menus) and its own commands (quit, save) without touching the core.
- **A key source that can poll**: today `keys_wait` blocks forever; add a
  non-blocking read and let widgets take the key source, so ade can keep its
  own state while a prompt is up.
- **The footer and header as front-end content**: `scr_footer` shows AED's
  fields and `title` is a global `"AED: Another Text Editor"`; make the text
  the caller's.

### aed after the refactor

`main`, AED's keymap, help text, banner, settings schema, header and footer --
and nothing else. Same binary behaviour, same keys, same files. The refactor is
invisible to AED's users.

## ade's first versions

1. **The proof**: ade links both libraries, opens a file from the command
   line and edits it exactly as aed does. If this works, the split works.
2. **Several documents**: a view per document, switch between them (a key and
   a window list), open and close. Two or three, per the memory and handle
   budget above; say so when the limit is reached.
3. **Menus**: a menu bar on the header row (File, Edit, Search, Build, Run,
   Options), built on the modal list helpers.
4. **Building through hub**: ade is a hub client -- the thing aed now won't
   be. On F9 it saves modified files, writes its session (open files, cursors,
   which is current) into a hub block, queues `acc ... -errors <f>` /
   `zap ... -e <f>` jobs with `HUB_STOP_ON_ERROR`, sets the continuation
   `ade -resume`, and exits. hub runs them and restarts ade, which restores
   the session and, if `hub_failed_job()` isn't -1, reads the error file and
   jumps to `file:line:col`. The hub design doc's "IDE" section spells out the
   flow; `hub.h` has the API.
5. **An error list** when a tool reports more than one line (both stop at the
   first error today, but the format allows more).
6. **Run**: Ctrl-F9 queues the build and then the program (`HUB_USER_PROGRAM`,
   `HUB_PAUSE_AFTER` are reserved in hub for this), returning to ade after.
7. **Projects**: an optional `project.ini` naming a program's files and build
   commands; a `[build]` section per file type.
8. **Later**: the user screen (Alt-F5, hub phase 7), dig integration
   (breakpoints set in ade), the look of a full IDE.

Note: ade and the tools all load at `0x40000`, so ade can't stay in memory
while acc or zap runs. It saves its session to hub and is restarted; that is
what hub is for. Start-up time therefore matters (AED's start-up has never
been measured; measure ade's early).

## The plan

### Phase A -- the library, in AED's repository (PRs to avalonbits/aed)

Each step is its own PR, behaviour-preserving, tested, verified in the
emulator on the oldest and newest firmware, reviewed, squash-merged. AED's
release cadence is unaffected.

| Step | Change | Done when |
|---|---|---|
| A1 | Core constants and paths out of UI headers; the app context (name, config/syntax/theme dirs) passed into core; no `"aed"` literal left in core | core compiles and its tests pass without `editor.h` |
| A2 | The `document` struct in core; `editor` holds one | all tests pass; aed unchanged |
| A3 | The `view` struct; `screen` becomes the device | a view can be drawn into a rectangle other than the full screen (a host test paints two) |
| A4 | `ed_translate`/`ed_handle`; the data-driven keymap with flags; sentinels gone; aed's loop in aed | aed's behaviour identical; a test drives a key through translate+handle |
| A5 | A polling key source; widgets take it | a test runs a prompt from a scripted, non-blocking source |
| A6 | The schema-driven config engine; AED's schema in aed | aed.ini round-trips byte for byte |
| A7 | The build: `core/` and `ui/` (or equivalent) producing `libedcore.a` and `libedui.a`; core-only tests linked against core objects only; `frames.sh` and `build_deps.sh` updated | aed links the two libraries; size within a few hundred bytes of before; the core-only test binary proves the layering |

### Phase B -- ade, in this repository

B1-B8 are the "first versions" list above, in that order. ade gets the
libraries as a pak, so **pak is built before phase B**: B1 (the proof) starts
once A7 has landed and aed's libraries can be packaged. Phase A does not wait
for pak.

## Decisions made

- **How ade gets the libraries**: as a **pak**. pak is built first; no
  submodule, vendored copy or path into `~/code/aed` in the meantime.
- **Library names**: **`libedcore`** and **`libedui`**.
- **Grammars and themes are shared**: ade reads `/config/aed/syntax` and
  `/config/aed/themes`. One copy serves both programs, so ade's install depends
  on aed's config directory.
- **ade needs hub and refuses to start without it**: on MOS older than 3.0.2,
  or when hub is not running, ade prints why and exits. There is no
  editor-only mode. aed's floor stays MOS 2.3.3, and **the libraries must keep
  working there**.
- **ade's session lives only in a hub block**: there's no file fallback. hub is
  always present when ade runs.

## Working rules

- **The user's global instructions** (`~/.claude/CLAUDE.md`) apply: commit after
  each passing change, every code change with a test that bites (break the line
  and watch it fail), the commit message format, **no AI attribution anywhere**,
  the Agon emulator rules (never type into the emulator's stdin; use
  `autoexec.txt`, a fifo, both output streams; stop at the MOS prompt), and the
  128-byte stack frame limit.
- **In AED's repository**, AED's rules (in `~/code/aed/.opencode/memory/` and
  `AGENTS.md`) apply: every change is a PR -- branch, PR with `gh`, address the
  Greptile review critically, squash-merge -- never a direct push to `main`;
  emulator checks on both the oldest (MOS 2.3.3 / VDP 1.04) and newest (MOS
  3.0.2 / VDP 2.16) firmware; never launch the emulator on the user's display
  (headless is fine); host tests with `test/run.sh` (ASan/UBSan), plus
  `test/frames.sh`, `test/docs.sh`, `test/build_deps.sh`.
- **In ade's repository** there is no remote yet: commit on `main` locally
  until the user sets one up.
- **Keep this briefing current** as decisions are made.
