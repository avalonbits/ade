# ADE: Another Development Environment

ADE is a development environment for the Agon platform (Agon Light, Agon
Light2, Agon Origins, Console8). It is built on AED's editor, using AED's
libraries, and adds menus, windows, projects, and building and running
through [hub](https://github.com/avalonbits/hub). You can edit, build, run,
fix the error and build again without leaving ADE.

What it does today:

- **Editing**: everything AED's editor does, including selection, copy and
  paste between files, undo, find, syntax colouring, settings, and files
  larger than memory.
- **Menus**: F10 opens a menu bar under the title with File, Edit, Search,
  Build, Run and Options. Each item shows its key.
- **Windows**: up to nine open files, three of them loaded at a time. Moving
  to a window that isn't loaded puts away one whose changes are saved.
- **Building**: CTRL+B builds the file on screen, or the whole project. A C
  file is built with [acc](https://github.com/avalonbits/acc), and assembly
  with [zap](https://github.com/avalonbits/zap). When a build fails, ADE comes
  back on the error, in its file, at its line and column.
- **Running**: CTRL+R builds and runs. ALT+F5 shows what the program left on
  the screen.
- **Projects**: a `project.ini` in the directory makes it a project. Every
  `.c`, `.s` and `.asm` file in it is built, objects go to `build/` and the
  program to `bin/`.
- **The shell**: CTRL+F12 drops you at hub's prompt, and `exit` brings you
  back to ADE as you left it.

There is no release yet: ADE is built from source (see
[Building ADE](#building-ade)).

## Contents

* [What you need](#what-you-need)
* [Installation](#installation)
* [Running ADE](#running-ade)
* [Keys](#keys)
* [Opening files](#opening-files)
* [Windows](#windows)
* [Building and running a file](#building-and-running-a-file)
* [Projects](#projects)
* [Errors](#errors)
* [The shell](#the-shell)
* [Settings](#settings)
* [Limits](#limits)
* [Building ADE](#building-ade)

## What you need

- **MOS 3.0.2.** ADE runs under hub, and hub needs it. VDP 1.04 or later, as
  for AED. ALT+F5 needs VDP 2.2.0 or later to capture the program's screen.
- **hub 0.6.0 or later**, in `/mos/hub.bin`. ADE refuses to start without
  hub: hub is what runs the tools and brings ADE back afterwards. CTRL+F12
  needs 0.6.
- **acc 0.4.0 or later**, to build C. A project's link uses acc's `@file`,
  which arrived in 0.4.0.
- **zap**, to build assembly, from zap's main branch for now. ADE asks zap for
  acc's object format (`-f acc`), which is newer than zap's latest release,
  1.1.0.
- **AED's configuration directory**, `/config/aed`, for syntax colouring,
  themes and fonts. ADE shares it with AED rather than keeping a copy. Without
  it, ADE works, but in plain text.

## Installation

1. **hub**: unzip `hub-<version>.zip` from
   [hub's releases](https://github.com/avalonbits/hub/releases) at the root of
   the SD card, which puts it in `/mos/hub.bin`. To have hub start at boot,
   make `hub` the last line of `/autoexec.txt`.
2. **acc**: unzip `acc-<version>.zip` from
   [acc's releases](https://github.com/avalonbits/acc/releases) at the root of
   the card, which gives you `/bin/acc.bin` and its library in `/lib/acc`.
3. **zap**: put `zap.bin` in `/bin`.
4. **AED's configuration**: unzip AED's release at the root of the card, which
   gives you `/config/aed` (AED itself comes with it).
5. **ADE**: copy `bin/ade.bin` from your build (see
   [Building ADE](#building-ade)) to `/bin/ade.bin` on the card.

If hub is already running when you install a new hub, type `exit` and then
`hub` to start the new one.

## Running ADE

At hub's prompt:

    ade                 start with no file
    ade hello.c         open hello.c

With no file named, in a project's directory, ADE opens the project's first
file: the first on its `files =` line, or else the first by name. With no
file and no project, ADE shows its banner, which says to press F10 for the
menus. The first key takes the banner away.

The screen has ADE's title on the top row and the menu bar under it. The
document is in the middle. The footer at the bottom shows the file, whether
it has unsaved changes, and the cursor's position. The right end of the
menu bar shows how the last build went, the project's program, and the open
windows, with the one on screen in brackets: `built  prog  1 [2] 3`.

## Keys

ADE adds these to AED's editing keys. The menus list them too.

| Key | What it does |
|---|---|
| F10 | Open the menus. LEFT and RIGHT move between them, ESC closes them |
| CTRL+O | Open a file in this window, from the file viewer |
| CTRL+SHIFT+O | Open a file in a new window, from the file viewer |
| CTRL+S | Save |
| CTRL+ALT+S | Save as |
| ALT+F3 | Close the window on screen |
| CTRL+TAB / CTRL+SHIFT+TAB | The next window / the one before |
| CTRL+1 to CTRL+9 | Window 1 to 9, if it is open |
| ALT+F6 | The list of windows |
| CTRL+SHIFT+N | New project here, or rename the one here |
| CTRL+B | Build |
| CTRL+R | Build and run |
| ALT+F9 | The errors the last build left |
| ALT+F5 | The screen the program left |
| CTRL+F12 | hub's shell; `exit` comes back |
| CTRL+E | Settings |
| CTRL+Q | Quit |

From AED: CTRL+Z undo, CTRL+Y redo, CTRL+X, CTRL+C and CTRL+V cut, copy and
paste, CTRL+A select all, CTRL+D delete the line, CTRL+F find, CTRL+N and
CTRL+P the next and previous match, and CTRL+G go to a line.

## Opening files

CTRL+O and CTRL+SHIFT+O open the file viewer. It lists the directory ADE was
started in, which is the project's directory when there is a project, with
directories first and then files, each in order of name.

- UP and DOWN choose, and RETURN opens the file or goes into the directory.
  `../` goes back up.
- Typing narrows the list to the names that contain what you type. The name
  bar shows what you've typed. When nothing is left in the list, RETURN
  opens the typed name as a new file.
- F7, or RETURN on the `[ New directory ]` row, asks for a name and makes a
  directory there.
- ESC leaves the viewer without opening anything.

The viewer stays inside the directory ADE was started in. It has no `../`
there, and it refuses a typed name that leads outside it.

CTRL+O opens the file in place of the one on screen, asking first whether to
save changes. A file that is already open in another window is shown there
instead. CTRL+SHIFT+O opens it in a new window.

## Windows

Each window is a file and a cursor. ADE keeps up to nine windows, numbered in
the order they were opened. Closing a window renumbers the ones after it.
Opening a file that is already in a window says which window it is in, and
opens nothing.

Three windows have their file loaded at any time. Going to a window that
isn't loaded loads it, and puts away the loaded window you used longest ago,
as long as its changes are saved. A window that is put away keeps its file
and its cursor, but not its undo history. If none of the loaded windows is
saved, ADE asks whether to save the one used longest ago. Answering N leaves
everything as it was. A file whose path is 64 characters or longer stays
loaded.

## Building and running a file

Without a `project.ini` in the current directory, CTRL+B builds the file on
screen, after saving every window:

- `name.c` with `acc name.c -o name.bin`
- `name.s` or `name.asm` with `zap name.s name.bin -c`

hub runs the tool, then starts ADE again. If the build worked, ADE comes back
with every window where it was and `built` on the menu bar, without waiting
for a key. If it failed, ADE comes back on the error; see [Errors](#errors).

CTRL+R builds, then runs the program, then waits for a key so you can read
what the program printed before ADE comes back. The menu bar then shows what
the program returned. ALT+F5 shows the program's screen again, in the screen
mode it left, until you press a key.

## Projects

A directory with a `project.ini` is a project. In a project, CTRL+B builds
every file in it and links the program, and CTRL+R runs that program.

**CTRL+SHIFT+N** (or **Build → New project**) writes a `project.ini` naming
the program, offering the name of the file on screen. When there is a
project here already, it asks for a new name instead. It changes only the
`name =` line, keeping the rest of `project.ini`, and deletes the old name's
program, `bin/<old>.bin`, and its object list, `build/<old>.lnk`, so they
aren't left behind. A project needs only the name:

    [project]
    name = hello

Without a `files =` line, every `.c`, `.s` and `.asm` file in the directory,
and in the directories under it (four levels deep), is part of the project:
`more/func.c` is built to `build/more/func.o`. Directories whose names start
with `.`, and the project's own `build/` and `bin/`, are left out. The other
settings are all optional:

    [project]
    name = hello                 ; the program: bin/hello.bin
    files = main.c util.c fast.s ; only these, in this order
    exclude = scratch.c          ; with no files line: all but these
    run = bin/%n -v data.txt     ; how CTRL+R runs it; bin/%n by default

    [build]                      ; how each kind of file is built, and the link
    c = acc -c %f -o build/%b.o -errors %e
    s = zap %f build/%b.o -f acc -c -e %e
    asm = zap %f build/%b.o -f acc -c -e %e
    link = acc @%l -o bin/%n.bin -errors %e

Those `[build]` lines are the defaults. Inside a command, these placeholders
are replaced:

| Placeholder | Replaced by |
|---|---|
| `%f` | the file being built |
| `%b` | that file without its extension |
| `%o` | every object, `build/<file>.o`, in order |
| `%l` | `build/<name>.lnk`, which lists every object, one per line, for acc's `@` |
| `%n` | the program's name |
| `%e` | the file a tool writes its errors to |
| `%%` | a percent sign |

An empty `link =` means there is no link step: a program assembled in one
piece, `s = zap %f %n.bin -c -e %e`, doesn't need one.

Objects go in `build/` and the program in `bin/`. ADE creates both
directories, so only your source is left beside `project.ini`. The build
itself is `build.obey`, which ADE writes beside `project.ini` and hub runs
with MOS's Obey. It stops at the first command that fails. ADE writes it
again only when `project.ini` changes, so an edit you make to it lasts until
then. You can also run it yourself at the prompt with `obey build.obey`.

## Errors

When a build fails, ADE opens the file the error is in, puts the cursor on
the error, and shows it on the bottom row as `line:column: message`. An
error that isn't in any file, such as a function called but never defined,
is shown as the tool wrote it.

When there is more than one error, ADE shows the list. Choosing one goes to
it, and ALT+F9 brings the list back until the next build.

## The shell

CTRL+F12 saves every window and drops you at hub's prompt, which shows `ADE *`
while it belongs to ADE. Everything works there as at hub's own prompt,
including running AED, ADE, or a build. `exit` brings ADE back, with every
window where it was.

## Settings

CTRL+E opens the settings: the colours, the tab width, the font, the cursor,
and the rest of AED's settings. ADE keeps them in `/config/ade.ini`, apart
from AED's, and writes that file the first time it runs.

## Limits

- A project has at most 32 files.
- A project's commands, after their placeholders are replaced, may be at most
  250 characters each: MOS reads no more than that from a line of
  `build.obey`. The link names its objects through `%l` for that reason.
- hub holds at most 93 characters for a command it runs. A single file's
  build command, and the `run` line, must fit in that.

## Building ADE

ADE builds on Linux with AgonDev (`agondev-config` on the `PATH`),
from the libraries of the AED release named in `aed-libs.pin`. A plain `make`
fetches that release's `aed-libs` package, checks its sha256, and builds
`bin/ade.bin`.

    make                         # bin/ade.bin
    make AED_LIBS=<dir>          # against an unpacked aed-libs package instead
    make HUB=<dir>               # hub's checkout, ~/code/devel by default

ADE links hub's client library, `build/lib/agondev/libhub.a`, from a hub
checkout.

`test/run.sh` builds ADE and runs its checks, most of them on
[fab-agon-emulator](https://github.com/tomm/fab-agon-emulator) 1.2.4
(`~/fab-agon-emulator-1.2.4`, or set `AGON_EMU`). A test build of ADE plays
scripted keys under hub, and the checks read back what it did: the screen,
the files, and what was built and run. The build cases need the Agon's acc
and zap (`ACC_BIN` / `ACC_DIR`, `ZAP_BIN`, by default their checkouts beside
this one). `test/frames.sh` checks that no function's stack frame is larger
than an `(ix+d)` offset can reach.

ADE is free software, under the GNU General Public License, version 3 or
later.
