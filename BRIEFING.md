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
| **hub** -- resident shell/scheduler | `~/code/devel`; design: https://claude.ai/code/artifact/2284d613-0e89-40fc-bf85-5e81dd591918 | runs acc/zap/the program for ade and brings ade back; keeps ade's session in a hub block; `include/hub/hub.h` + `build/lib/agondev/libhub.a` |
| **zap** -- assembler | `~/code/zap`; `-e <file>` on branch `error-file`, PR avalonbits/zap#163 | assembling; its error file |
| **acc** -- C compiler | `~/code/acc` (another agent works there; read only); `-errors <file>` with columns on branch `error-file`, worktree `~/code/acc-diag` | compiling; its error file |
| **pak** -- library packaging (designed, not built) | https://claude.ai/code/artifact/ddf7e7fe-e8eb-4e6e-b4a4-6c79aae6962a | postponed; would later replace downloading the libraries from aed releases |
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
| A6 | The schema-driven config engine; AED's schema in aed, including the text `cfg_render` writes at the top of a new settings file (it says "AED" and names `/config/aed/unscii8x9.bin` -- left in core by A1) | aed.ini round-trips byte for byte |
| A7 | The build: `core/` and `ui/` (or equivalent) producing `libedcore.a` and `libedui.a`; core-only tests linked against core objects only; `frames.sh` and `build_deps.sh` updated | aed links the two libraries; size within a few hundred bytes of before; the core-only test binary proves the layering |

### Phase A progress

- **A1** -- avalonbits/aed#180, merged. The core's `app_context` (`src/app.h`)
  holds the program's name, settings file and its old name, and its syntax,
  theme and font directories. The front end hands it over once with `app_set`;
  aed's is `AED_APP` in `editor.c`, set first thing in `ed_init`. With no
  context, an unnamed document's store and clipboard spill fail rather than
  guess a name. `cfg_migrate(old, path)` takes its paths. `AED_DOC_KB` is now
  `TB_DOC_KB` in `text_buffer.h`. The `.aedh`/`.aedt`/`.aeds` suffixes stay:
  they are the core's scratch format, shared by both programs. ade will pass
  `ade` as its name and the `/config/aed/...` directories (grammars and themes
  are shared).
- **A2** -- avalonbits/aed#181, merged. `document` (`src/document.h`, core):
  buffer, undo log, grammar (`syn_`), syntax window, selection. The editor
  holds one as `doc_`. **The theme is the screen's, not the document's**: it is
  picked by the background, so a background change must re-pick it for every
  document on screen -- it stays in `editor` beside `scr_`. **The selection is
  per document on purpose**: two views of one file share it.
- **A3** -- avalonbits/aed#182, merged. `view` (`src/view.h`, UI): rectangle,
  cursor, `originX_`, `synTop_`. `screen` is the device and paints through
  `v_`; it lays out `whole_` (the whole text area) and starts pointed at it;
  `scr_set_view` switches, NULL returns to `whole_`. The footer is on the
  screen's last row. Help, banner and settings draw on `whole_` and restore
  the caller's view -- **but they paint over every pane, so ade must repaint
  all its views after one closes**, not just the current one. A font or mode
  change re-lays `whole_` only; other views are their owner's to re-lay. A
  screen must not be copied (`v_` may point into its own `whole_`).
- **A4** -- avalonbits/aed#183, merged. Keys through a keymap (`key_binding`
  table: vkey, mods, flags, cmd; `AED_KEYS` is aed's, the editor reads
  `keys_`). Flags: `KC_OWNS_SEL` (binding), `KC_EDITS` (the key), `KC_PUTC`
  (typed character). `ed_translate(keymap, key_press)` is pure;
  `ed_handle(editor, key_command)` runs a key and returns false once a command
  set `leaving_`. Quit/save are ordinary bindings (`ed_cmd_quit`,
  `ed_cmd_save`). **ade puts its own table in front of AED's through the
  keymap's `next`** rather than copying it; a NULL keymap binds nothing.
- **A5** -- avalonbits/aed#184, merged. `keys_poll` is non-blocking; a
  `key_source` is `{ poll, idle, ctx }` (`KEYS_MOS` is the keyboard) and
  `ks_wait` runs `idle` between polls. Prompts read `ui_.keys_`
  (`ui_set_keys`), and so does `ed_run`. **An idle runs while a prompt owns the
  screen: it must not paint or change the document** -- counting, polling hub,
  noting things for later.
- **A6** -- avalonbits/aed#185, merged. `config.c` is a schema engine
  (`cfg_schema`: settings table by `offsetof`, plus a render); AED's schema is
  `AED_CONFIG` in `aed_config.c`. ade brings its own schema for `ade.ini`.
  AED's file is pinned byte for byte by `test_config_golden`.
- **A7a** -- avalonbits/aed#186, merged. AED's help, banner, settings modal,
  help/settings commands, `AED_APP` and `AED_KEYS` live in `aed.c`/`aed_ui.c`.
  The modal helpers are exported (`ui_modal_fill`, `ui_modal_title`,
  `ui_modal_move`, `ui_put_at`, `ui_pad_to`, `ui_put_num`, `ui_font_picker`)
  for ade's menus and lists. **An editor starts with
  `ed_init_for(ed, kb, fname, prog)`**, where `ed_program` is `{ app, keys,
  settings, banner }`: settings runs once the screen exists and returns a
  message to report (or NULL); banner shows when starting with nothing. No
  program state lives outside the editor. `ed_init` is AED's wrapper.
- **A7b** -- avalonbits/aed#187, merged: **Phase A is done.** `src/core` builds
  `libedcore.a`, `src/ui` builds `libedui.a` (`make libs`), and `src` is AED,
  linked `-ledui -ledcore`. The host suite compiles each layer with only the
  layers below it on the include path and links core-only and UI-only tests
  against those layers alone, so a layering violation fails the build.
- **Follow-ups from the A7b review** (small, not yet done; each needs the
  user's go-ahead for the aed session to merge): assert from `bin/aed.map`
  that the link really comes from both archives and nothing loose; make
  `make lib` (which now archives only AED's objects) point at `libs`, or
  document `libs`; guard `test/run.sh` against a `core/x.c` and `ui/x.c`
  sharing a basename.
- **Review loop**: each Phase A PR was reviewed by the aed session (in
  `~/code/aed`), fixed until it was satisfied, and merged by it; the user's
  clearance covered A2-A7 and has ended. Anything after Phase A needs the
  user's go-ahead before it is merged.
- **Device checks for aed PRs**: there is no Xvfb here, so the full emulator
  runs with `SDL_VIDEODRIVER=dummy` and results are read from the card. A
  probe -- the real `src/*.c` with `main.c` swapped for a driver that writes
  `/probe.out` -- built against the branch and against main as a control, run
  on MOS 3.0.2 / VDP 2.16 and on MOS 2.3.3 (downloaded with `gh api`, with
  `MOS.map` beside it) / VDP 1.04 (`--firmware quark`). Only the first line of
  `autoexec.txt` runs.

### Phase B -- ade, in this repository

B1-B8 are the "first versions" list above, in that order. ade gets the
libraries from an aed release (see the decisions below), so phase B needs an aed
release that carries `aed-libs-<version>.tar.gz`; until one exists, B1 builds
against a package made locally with aed's `mklibs.sh`.


### Phase B progress

- **aed v1.3.2** was the first release carrying `aed-libs-<version>.tar.gz`;
  **v1.3.3** adds aed#190's base keymap (`ED_KEYS` in libedui holds the 51
  editing bindings; `AED_KEYS` is aed's own six keys chained in front), which
  ade's keys chain in front of the same way. ade pins **v1.3.3**
  (`aed-libs.pin`, sha256 `d3e0db62...7e71`); `tools/fetch-aed-libs.sh`
  downloads, checks and unpacks it into `third_party/`, and a plain `make`
  builds ade. `AED_LIBS=<dir>` is still there for an unreleased aed.
- **B1 is written** (`src/main.c`): ade opens the file on its command line and
  edits as aed does -- its own app context (`ade`, `/config/ade.ini`, aed's
  grammar/theme/font directories), its own keys (open, save, save-as, quit)
  in front of `ED_KEYS`, no settings or banner yet. `test/run.sh` checks the
  fetch, the build and link, and on the emulator on both firmwares that ade
  opens a file named on its command line. 82,258 bytes.
- **aed#192** is merged as `9802c17`, after v1.3.3: the editor points at
  its current document (`doc_`; its own is `home_`), with `ed_doc_open`,
  `ed_doc_show`, `ed_doc_close` and `scr_view_init` for programs that keep
  several. A file too big for memory can be paged by only one document at a
  time -- a second open, by any spelling of its path, is `TB_IN_USE` -- since
  two stores on one file would share and destroy its scratch files. It also
  added `aed-libs-*.tar.gz` to aed's `.gitignore`. It shipped in **v1.3.4**,
  which ade now pins (sha256 `7f2d0e06...cabc`); a plain `make` builds ade.
- **B2 is written** (`src/windows.c`): up to three windows, full-screen, one
  on screen at a time. Window 1 is the file ade started with and stays open;
  48 KiB each. F3 opens a file in a new window (and says so at three, or when
  that file is already in a window -- names are resolved, so `a.txt` and
  `/a.txt` are one); F6 next; ALT+F3 closes the one shown, asking about
  unsaved work, and goes back to the previous; ALT+F6 lists them; CTRL+Q asks
  about every unsaved window. `test/run.sh` plays 42 keys through a test
  build (`test/script`) on both firmwares and checks what each window holds.
  Window state is file-scope in `windows.c` -- one ade, one set of windows.
- **aed#194** (merged by the user as `9ba06c4`, shipped in **v1.3.5**, which
  ade pinned then -- sha256 `ca70e812...a56c`): the header row is the program's.
  `scr_set_title`, `scr_set_header(draw, ctx)` (called on each clear in the
  title's place) and `scr_header_draw`; `ed_program.title` is a new last
  field, and a program that leaves it out gets no title.
- **B3 is written** (`src/menu.c`): the menu bar on the header row -- File,
  Edit, Search, Build, Run, Options -- drawn by ade's header hook, the open
  menu in brackets. F10 opens it; LEFT/RIGHT move between menus (wrapping),
  UP/DOWN through the pulled-down one, RETURN chooses, ESC or F10 leaves.
  Each pull-down is drawn in a view of its own under its name, with the
  document's colouring set aside. Every entry is also a key, shown beside
  it, and choosing it runs through `ed_handle` as the key would; F10 owns the
  selection so Copy and Cut still have one. Build, Run and Options say what
  they are waiting for. The scripted run (61 keys) now also undoes from the
  Edit menu, tries Build, wanders the bar, and quits from the File menu.
- **B4 is written** (`src/build.c`): F9, or Build on the menu, builds the file
  on screen -- `.c` with acc, `.s`/`.asm` with zap -- through hub. ade saves
  every changed window (one with no name stops it), keeps the session in
  hub's block `ADE ` (which files are open in which windows, each cursor, the
  window on screen), pushes the tool with `HUB_STOP_ON_ERROR`, asks to come
  back as `ade -resume`, and returns. Resumed, it reopens the windows, puts
  each cursor back, and either says "Built" or reads the first line of
  `/ade.err` (`file:line:col: ...`, both tools' form), goes to that file's
  window -- opening it if it has none -- at the error, and shows it.
  ade now **refuses to start without hub** (`hub_present`, which is also what
  lets every other hub call work), so every emulator check runs under hub on
  MOS 3.0.2, started from `hub -f /script.txt`; the refusal is checked on
  3.0.2 and 2.3.3. The build checks put acc (with `/lib/acc`) and zap on the
  card and run a broken C file, a broken assembly file, and a good C file
  built from window 2 with both windows changed and unsaved. The build and
  resume messages wait for a key, so the test's keys go in before the resume.
  Linking `libhub.a` and using `snprintf` took ade to 98,815 bytes.
- **Start-up is measured** (`test/run.sh`, at the Agon's own speed, from
  `main` to ade's first key; MOS loading the binary is not included, since the
  emulator reads the card from the host). ade starts in **0.42 s**. A resume
  is the start plus 0.2 s to open each further window plus 0.2 s per full
  paint: showing a window is one, placing its cursor another, since
  `cmd_restore_after_modal` recentres by clearing and repainting. Two windows
  came back in 1.60 s; placing the window that ends up on screen last, once,
  and not repainting after the message, brought that to 1.40 s. aed 1.3.7's
  `ed_doc_place` now lets every window be placed without drawing and only
  the one ade ends up on be painted, which brought it to **0.84 s**.
  `test/run.sh` holds it under 1.00 s.
- **B7 is written** (`src/project.c`): a `project.ini` in the current
  directory makes F9 build the project, whatever is on screen. `[project]`
  has `name` (the program, `name.bin`) and `files` (up to 32, built in
  order, then linked). `[build]` may replace the rules for `c`, `s`, `asm`
  and `link`; the defaults are `acc -c %f -o %b.o -errors %e`,
  `zap %f %b.o -f acc -c -e %e` and `acc %o -o %n.bin -errors %e` (acc on
  the Agon links `/lib/acc/libc.a` by itself). `%f` is the file, `%b` the
  file without its extension, `%o` every object, `%n` the name, `%e` the
  error file. An empty `link` runs none. The commands go into `build.obey`
  beside `project.ini`, and hub runs `Obey build.obey` as a single job with
  `HUB_STOP_ON_ERROR`. MOS 3.0.2's Obey stops at the first line that fails
  and returns its error. So a project isn't held to hub's `HUB_MAX_JOBS` (8),
  and Run is three jobs: the build, the program, and ade. A command may be 250
  characters long, which is what fits in Obey's 256-byte line. `build.obey`
  is written again only when `project.ini` changes. Its first line is a
  comment with an FNV-1a checksum of `project.ini` and of ade's default
  rules, so a hand edit lasts until then, and it runs at MOS's prompt too.
  One job per file came first, and it capped a project at five. A project that can't be
  built (no name, no files, a file nothing builds, a command too long for
  `HUB_CMD_MAX`) says why before anything is saved. The checks:
  - C plus assembly, built with the defaults and then run by hub;
  - an error in a file that isn't on screen, which ade opens in a window of
    its own;
  - a file nothing builds;
  - an assembly program with its own rule and no link;
  - eight files, built, linked and run;
  - a hand edit to `build.obey` that lasts while `project.ini` is unchanged
    and is gone once a file is added.
- **B5 is written** (`build_errors` in `src/build.c`). After a failed build,
  ade reads every `file:line:col:` line of `/ade.err`, up to 16. With one
  error it goes to it and shows it, as before. With more, it goes to the
  first and opens an ERRORS list over it: RETURN goes to the chosen error
  (opening its file in a window if needed) and shows it, and ESC stays.
  ALT+F9, also Errors on the Build menu, brings the list back at the last
  error chosen. acc and zap still stop at one error, so the check uses
  `test/tools/fail.c`, built with host acc: it writes three errors and fails
  with 100, as a project's `c` rule.
- **hub 0.3** (`798267e` in `~/code/devel`, done by the hub session at ade's
  request) acts on `HUB_USER_PROGRAM`: before the job it resets the cursor
  settings, the system font and the mode noted at its prompt. It also acts on
  `HUB_PAUSE_AFTER`: after the job it prints "Press a key to return" and
  waits. A script sets `Hub$NoPause 1` to go on without waiting. The same
  commit frees the running job's slot. Before it, ade started by hub could
  queue only 7, so a six-file F9 after the first build was `HUB_ERR_FULL`.
  The header is now `<hub/hub.h>`, and the library
  `build/lib/agondev/libhub.a`.
- **B6 is written** (`build_run` in `src/build.c`). CTRL+F9, also Run on the
  menu, queues the build, then the program with `HUB_USER_PROGRAM |
  HUB_PAUSE_AFTER` and without `HUB_STOP_ON_ERROR`, then `ade -resume`. The
  program is the project's `name`, or the file's name without its extension,
  run from the current directory. A project's `run` line says how, arguments
  and all (`run = %n -v data.txt`), with `%n` alone the default. hub skips it when a build step fails, and
  ade comes back on the error. Otherwise ade says "<program> returned N". The
  session remembers whether it ran (magic `ADE2`). The checks cover:
  - Run with `Hub$NoPause`: the program runs and ade comes back;
  - Run without `Hub$NoPause`: the program runs and ade does not come back,
    which is how the test sees that ade asks for the pause;
  - Run on a broken file: ade comes back on the error, and a stale program
    that would leave a mark is not run.

  No check sees `HUB_USER_PROGRAM` itself. The test can't read the VDP's
  state, and hub's own tests cover the reset.
- **aed v1.3.7** (tag `6811f32`, aed-libs sha256 `7c5a62e6...add1`), which ade
  now pins, adds two things to libedui at ade's request: aed#200 moves the
  editor's settings into the library (`settings.h`: `ED_SETTINGS`,
  `ed_settings_apply`, `ed_cmd_settings`, `ed_settings_modal`,
  `ed_settings_render`, each taking the program's `cfg_schema`), and aed#201
  adds `ed_doc_place`, which places a document's cursor and view without
  drawing. aed#201 also fixed `cmd_restore_after_modal`, which dropped the
  view's sideways scroll.
- **The resume's cursor was drawn in the wrong column.** It recentred each
  window with `cmd_restore_after_modal(ed, true)`, which keeps the screen
  cursor's old column: on a fresh start, column 1. So coming back on an error
  at 3:12 drew the cursor at the start of line 3 until the next key. The
  tests read the cursor from the text, so they could not see it. The resume
  now uses `ed_doc_place`, and the scripted report gives the screen cursor
  too (`screen row:col`), which must agree with the text's (`at`).
- **Settings** (`src/options.c`): the editor's settings (tab width, colours,
  a font, from aed's library) in ade's own `/config/ade.ini`, which the first
  run writes. Options -> Settings and CTRL+E change them. ade's part is its
  schema, a render naming ade, and one-line wrappers. Linking the settings
  screen and its pickers took ade from 104,581 to 113,154 bytes.
- **B8, as far as it can go now.** The header row's right-hand end shows the
  project's program, from `project.ini`, and the open windows with the one
  on screen in brackets (`prog  1 [2] 3`). The footer already has the file,
  its changed mark and the position.
  - **ALT+F5, the user screen** (`build_user_screen`, also Run -> User
    screen), on hub 0.4's phase 7 (`42df6fb` in `~/code/devel`). hub
    captures the screen into VDP buffer `HUB_SCREEN_BUFFER` (0x4855; hub
    reserves 0x4800-0x48FF) as a `HUB_USER_PROGRAM` job ends, and
    `hub_user_screen()` gives the mode, or -1 with no capture. ade switches
    to that mode if it differs, draws the buffer at 0,0 in logical
    coordinates, waits for a key, switches back and reapplies its settings
    (a mode change drops the font and colours), and repaints. A capture
    needs Console8 VDP 2.2.0 or later; on an older VDP the buffer is empty.
    The test runs a program that clears the screen to red and reads a pixel
    back from the full emulator's VDP with VDU 23,0,&84, while the user
    screen is up and again after: `aa0000`, then ade's black.
  - **A program that changed the screen mode left ade in it.** hub resets
    the mode before a `HUB_USER_PROGRAM` job and leaves it alone after, so
    a program that switched to mode 3 had ade resume in mode 3. The session
    now records ade's mode (magic `ADE3`), and `build_resume_screen` puts it
    back before `ed_init_for` measures the screen. The checks run a program
    in mode 3 too: ade ends in mode 0, hub captured in 3, and ALT+F5 shows
    the red in 3 and returns.
  - Breakpoints wait on dig, which is a briefing only, with editor
    integration down as "Later"; C source lines also need acc line tables.
  - A link line longer than 250 characters, the Obey limit, has no way
    round it inside ade. acc reads each archive once, in order, so splitting
    the objects over archives breaks back-references. The fix is acc taking
    its inputs from a file (`acc @list`), which is acc's to do.
- **Memory is measured**: with three windows open, each paging a file too
  big for it, 34 KB is left for the heap and stack, which share one region.
  `test/run.sh` fails below 16 KB, and a 24 KB leak takes it to 10.
- **aed v1.4.1** (tag `6e766cc`, sha256 `c4e338fb...e865`), pinned, carries
  two library changes from ade's testing on Igor's emulator. aed#212 keeps a
  message or question to the prompt row: written whole, a long one wrapped
  off the bottom row and the VDP scrolled the screen up a line. ade already
  cuts its own messages, and drops an error's position, since the cursor is
  on it. aed#213 adds `scr_set_subheader`, a row of the program's own under
  the header, with the text from row 2. ade's title now keeps the top row,
  with the menu bar under it, attached from ade's `ed_program.settings` hook
  so the editor's first paint is already laid out that way. Attaching it
  after `ed_init_for` cost a second full paint: start-up went from 0.46 s to
  0.64 s, which the timing check caught.
- **Stack frames are checked** (`test/frames.sh`, aed's check over ade's
  sources): the largest is 71 bytes (`menu_open`), with no escapes.
## Decisions made

- **How ade gets the libraries**: from **aed's GitHub release assets**. Each aed
  release carries `aed-libs-<version>.tar.gz` (made by aed's `mklibs.sh`:
  `lib/libedcore.a`, `lib/libedui.a`, `include/core/*.h`, `include/ui/*.h`,
  `VERSION`). ade pins a version and its sha256 and downloads it at build time;
  `AED_LIBS=<dir>` points the build at an unpacked package instead, for working
  against an unreleased aed. **pak is postponed** (decided 2026-09-28): it was
  the first choice, and would replace this download when it exists.
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
