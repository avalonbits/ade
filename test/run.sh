#!/bin/bash
# ade's checks.
#
#   1. tools/fetch-aed-libs.sh takes the pinned package and nothing else.
#   2. ade builds against aed's two libraries, and links from them.
#   3. On the emulator: without hub, on MOS 2.3.3 and 3.0.2, ade says it
#      needs it and stops. Under hub, on MOS 3.0.2 -- hub's floor -- ade
#      opens a file named on its command line, plays a session of windows
#      and menus, and builds: acc and zap fail on a broken file and ade comes
#      back on the error, and a good file is built.
#
# The build and the emulator need a package: AED_LIBS=<unpacked package>, or
# the one aed-libs.pin names, fetched; and hub -- HUB=<checkout>, ~/code/devel
# by default -- for its libhub.a and hub.bin. The emulator run needs
# ~/fab-agon-emulator-1.2.4 (AGON_EMU overrides) and is skipped without it;
# the oldest firmware's MOS 2.3.3 is fetched from its release once and kept.
# The builds use the Agon acc and zap: ACC_BIN and ZAP_BIN, by default their
# repositories' bin/.
set -uo pipefail
cd "$(dirname "$0")/.."

status=0
check() {
    if [ "$2" = "$3" ]; then
        printf 'PASS  %-56s %s\n' "$1" "$2"
    else
        printf 'FAIL  %-56s got %s, want %s\n' "$1" "$2" "$3"
        status=1
    fi
}

W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

# --- 1. fetching the pinned package ---
{
    # A package of our own making, served from a file:// URL, into a scratch
    # copy of the repo so the real pin and third_party/ are left alone.
    mkdir -p "$W/pkgsrc/aed-libs-9.9.9/lib" "$W/pkgsrc/aed-libs-9.9.9/include/core"
    echo marker > "$W/pkgsrc/aed-libs-9.9.9/lib/libedcore.a"
    tar -C "$W/pkgsrc" -czf "$W/good.tar.gz" aed-libs-9.9.9
    echo tampered >> "$W/pkgsrc/aed-libs-9.9.9/lib/libedcore.a"
    tar -C "$W/pkgsrc" -czf "$W/bad.tar.gz" aed-libs-9.9.9
    GOOD=$(sha256sum "$W/good.tar.gz" | cut -d' ' -f1)

    R="$W/repo"
    mkdir -p "$R/tools"
    cp tools/fetch-aed-libs.sh "$R/tools/"
    pin() { printf 'version 9.9.9\nsha256 %s\n' "$1" > "$R/aed-libs.pin"; }

    pin pending
    AED_LIBS_URL="file://$W/good.tar.gz" "$R/tools/fetch-aed-libs.sh" >/dev/null 2>&1
    check "a pin with no checksum yet is refused" "$?" 1

    pin "$GOOD"
    got=$(AED_LIBS_URL="file://$W/bad.tar.gz" "$R/tools/fetch-aed-libs.sh" 2>/dev/null)
    check "a package that does not match the pin is refused" "$?" 1
    check "  and nothing of it is left" "$(ls "$R/third_party" 2>/dev/null | wc -l)" 0

    got=$(AED_LIBS_URL="file://$W/good.tar.gz" "$R/tools/fetch-aed-libs.sh" 2>/dev/null)
    check "the pinned package is unpacked" "$got" third_party/aed-libs-9.9.9
    check "  in full" "$(head -1 "$R/$got/lib/libedcore.a")" marker
    got=$(AED_LIBS_URL="file://$W/nowhere.tar.gz" "$R/tools/fetch-aed-libs.sh" 2>/dev/null)
    check "  and found in place after, without downloading" "$got" third_party/aed-libs-9.9.9
}

# --- 2. the build ---
if ! command -v agondev-config >/dev/null; then
    echo "SKIP  build: agondev-config not on PATH"

    exit $status
fi
LIBS_DIR=${AED_LIBS:-$(tools/fetch-aed-libs.sh 2>/dev/null)}
if [ -z "$LIBS_DIR" ] || [ ! -d "$LIBS_DIR/lib" ]; then
    echo "FAIL  build: no aed libraries -- set AED_LIBS or fill in aed-libs.pin"

    exit 1
fi
make clean >/dev/null 2>&1
make AED_LIBS="$LIBS_DIR" >/dev/null 2>&1
check "ade builds against aed's libraries" "$([ -f bin/ade.bin ] && echo built)" built
check "  linking the core from libedcore.a" \
      "$(grep -q 'libedcore\.a(' bin/ade.map && echo yes)" yes
check "  and the editor from libedui.a" \
      "$(grep -q 'libedui\.a(editor\.o)' bin/ade.map && echo yes)" yes
AED_LIBS="$LIBS_DIR" test/frames.sh || status=1

# Another package -- here the same one under another path -- and the next
# make builds and links against it, and back again, with no make clean
# between: the objects record the package they were built with.
cp -r "$LIBS_DIR" "$W/other-libs"
make AED_LIBS="$W/other-libs" >/dev/null 2>&1
check "another aed package: ade is relinked against it" \
      "$(grep -q 'other-libs/lib/libedui\.a(' bin/ade.map && echo yes)" yes
make AED_LIBS="$LIBS_DIR" >/dev/null 2>&1
check "  and back, against the pinned one" \
      "$(grep -q 'other-libs' bin/ade.map || echo yes)" yes

# --- 3. on the emulator ---
EMU=${AGON_EMU:-$HOME/fab-agon-emulator-1.2.4}
if [ ! -x "$EMU/fab-agon-emulator" ]; then
    echo "SKIP  emulator: none at $EMU"

    exit $status
fi
HUB=${HUB:-$HOME/code/devel}
ACC_DIR=${ACC_DIR:-$HOME/code/acc}
ACC_BIN=${ACC_BIN:-$ACC_DIR/bin/acc.bin}
ZAP_BIN=${ZAP_BIN:-$HOME/code/zap/bin/zap.bin}

# MOS 2.3.3, the oldest aed supports, with the map beside it: without the map
# the emulator quietly disables the host SD card. Fetched once through the
# asset API -- `gh release download` exits 0 here and writes nothing.
MOS233=third_party/mos-2.3.3
if [ ! -s "$MOS233/MOS.bin" ] || [ ! -s "$MOS233/MOS.map" ]; then
    mkdir -p "$MOS233"
    gh api -H "Accept: application/octet-stream" \
        repos/AgonPlatform/agon-mos/releases/assets/237714140 > "$MOS233/MOS.bin" 2>/dev/null
    gh api -H "Accept: application/octet-stream" \
        repos/AgonPlatform/agon-mos/releases/assets/237714161 > "$MOS233/MOS.map" 2>/dev/null
fi
check "MOS 2.3.3 is at hand" "$(wc -c < "$MOS233/MOS.bin" | tr -d ' ')" 81072

# card <ade.bin>: a fresh card with MOS's files, hub, ade, acc and zap.
card() {
    local c
    c=$(mktemp -d -p "$W")
    mkdir -p "$c/bin"
    cp -r "$EMU/sdcard/mos" "$c/"
    cp "$EMU/sdcard/MOS.bin" "$EMU/sdcard/firmware.bin" "$c/"
    cp "$HUB/build/hub.bin" "$c/mos/"
    cp "$1" "$c/bin/ade.bin"
    # acc links against its libraries on the card, where it looks for them.
    if [ -f "$ACC_BIN" ]; then
        cp "$ACC_BIN" "$c/bin/acc.bin"
        mkdir -p "$c/lib/acc"
        cp "$ACC_DIR/bin/libc.a" "$ACC_DIR/bin/rt.a" "$c/lib/acc/"
        cp -r "$ACC_DIR/include" "$c/lib/acc/include"
    fi
    [ -f "$ZAP_BIN" ] && cp "$ZAP_BIN" "$c/bin/zap.bin"
    echo "$c"
}

# under_hub <card> <seconds> <command> [paced]: boot MOS 3.0.2 with hub
# running <command> from a script, as a person would at hub's prompt, and stop
# the emulator after <seconds>. Whatever ade wrote is on the card. Run flat out
# (-u) -- acc is a quarter of a megabyte to load and run -- unless "paced":
# then at the Agon's own speed, for timing.
under_hub() {
    local c=$1 secs=$2 fifo hold speed=-u
    [ "${4:-}" = paced ] && speed=
    printf '%s\n' "$3" > "$c/script.txt"
    printf 'hub -f /script.txt\r\n' > "$c/autoexec.txt"
    chmod -R u+w "$c"
    fifo=$(mktemp -u -p "$W")
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" &
    hold=$!
    (cd "$EMU" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout "$secs" ./fab-agon-emulator $speed --sdcard "$c" < "$fifo" > /dev/null 2>&1)
    kill "$hold" 2>/dev/null
    wait "$hold" 2>/dev/null
}

# --- without hub, ade says so and stops ---
# Nothing is drawn before the check, so the CLI emulator's console shows it.
for fw in 3.0.2 2.3.3; do
    c=$(card bin/ade.bin)
    printf 'alpha\r\n' > "$c/a.txt"
    printf 'ade a.txt\r\n' > "$c/autoexec.txt"
    chmod -R u+w "$c"
    mos=()
    [ "$fw" = 2.3.3 ] && mos=(--mos "$PWD/$MOS233/MOS.bin")
    fifo=$(mktemp -u -p "$W")
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" &
    hold=$!
    out=$(cd "$EMU" && timeout 20 ./agon-cli-emulator -u --sdcard "$c" "${mos[@]}" \
        < "$fifo" 2>&1 | tr -d '\r')
    kill "$hold" 2>/dev/null
    wait "$hold" 2>/dev/null
    check "without hub, on MOS $fw, ade says it needs it" \
          "$(grep -c 'ade runs under hub' <<< "$out")" 1
done

# --- under hub: a file named on the command line ---
# A document too big for the memory ade gives one, so opening it pages -- and
# paging leaves its two scratch files beside it on the card while ade runs.
# The emulator is stopped with ade still in its key loop, so they are there
# to find: ade started, took the file from the command line, and opened it.
c=$(card bin/ade.bin)
for i in $(seq 1 3000); do
    printf 'line %04d of a document too long for memory\r\n' "$i"
done > "$c/big.txt"
under_hub "$c" 30 "ade big.txt"
check "under hub, ade opens a file named on its command line" \
      "$([ -f "$c/big.txt.aedh" ] && [ -f "$c/big.txt.aedt" ] && echo opened)" opened

# --- the test build ---
# ade with test/script in place of the keyboard -- see test/script/script.c
# for the keys, which depend on how ade was started -- as a project of its
# own, so the real build is left as it is.
case "$LIBS_DIR" in /*) LIBS_ABS=$LIBS_DIR ;; *) LIBS_ABS=$PWD/$LIBS_DIR ;; esac
proj="$W/scripted"
mkdir -p "$proj/src"
cp src/*.c src/*.h test/script/*.c test/script/*.h "$proj/src/"
cat > "$proj/Makefile" <<MK
NAME=ade
include \$(shell agondev-config --makefile)
CFLAGS += -DADE_SCRIPT -I$LIBS_ABS/include/core -I$LIBS_ABS/include/ui -I$HUB/include
PROJECTLIBDIR := $LIBS_ABS/lib
LIBS := -ledui -ledcore $HUB/build/lib/agondev/libhub.a
MK
(cd "$proj" && make >/dev/null 2>&1)
check "the scripted ade builds" "$([ -f "$proj/bin/ade.bin" ] && echo built)" built

# header <card>: the right-hand end of the header row as ade finished -- the
# project's program and the open windows -- past the menus' names.
header() {
    tr -d '\r' < "$1/ade.head" 2>/dev/null | awk -F'   +' '{print $NF}'
}

# played <card> <what> <want>: /ade.out, written as ade finished, against <want>.
played() {
    local got
    got=$(tr -d '\r' < "$1/ade.out" 2>/dev/null)
    if [ "$got" = "$3" ]; then
        printf 'PASS  %-56s %s\n' "$2" "as played"
    else
        printf 'FAIL  %-56s\n' "$2"
        echo "      want: $(echo "$3" | tr '\n' '|')"
        echo "      got:  $(echo "${got:-nothing}" | tr '\n' '|')"
        status=1
    fi
}

# --- under hub: windows and menus ---
c=$(card "$proj/bin/ade.bin")
printf 'alpha\r\n' > "$c/a.txt"
printf 'bravo\r\n' > "$c/b.txt"
printf 'charlie\r\n' > "$c/c.txt"
under_hub "$c" 40 "ade a.txt"
played "$c" "under hub, windows and menus, played" "keys 61 of 61
current 2
at 1:4
screen 1:4
window 1 a.txt 1:1 alpha
window 2 b.txt 1:4 y2#bravo
window 3 closed"
check "  and the header shows the open windows, 2 on screen" "$(header "$c")" "1 [2]"
check "  and the first run wrote ade's settings file" \
      "$(head -1 "$c/config/ade.ini" 2>/dev/null | tr -d '\r')" "# ade settings."
check "  and nothing was saved, as answered" \
      "$(cat "$c/a.txt" "$c/b.txt" "$c/c.txt" | tr -d '\r' | tr '\n' ' ')" \
      "alpha bravo charlie "

# --- settings ---
# The editor's settings, from aed's library, in /config/ade.ini: applied as
# ade starts...
c=$(card "$proj/bin/ade.bin")
mkdir -p "$c/config"
printf '[editor]\r\ntab = 2\r\n' > "$c/config/ade.ini"
printf '\tx\r\n' > "$c/tabs.txt"
under_hub "$c" 40 "ade tabs.txt"
played "$c" "settings: the file's tab width, applied as ade starts" "keys 2 of 2
current 1
at 1:3
screen 1:4
window 1 tabs.txt 1:3 	x
window 2 closed
window 3 closed"
# The menu bar in the footer's colours, to its last column: the scripted ade
# reads that cell's pixel back from the VDP as it finishes, with the footer's
# last and the text area's under the bar. Here, since this run leaves
# without a question on the footer's row.
bar=$(tr -d '\r\n' < "$c/ade.bar" 2>/dev/null)
check "the menu bar is in the footer's colours, not the text's" \
      "$(set -- $bar; [ -n "$2" ] && [ "$2" = "$6" ] && [ "$2" != "$4" ] && echo same)" same

# ...and changed with CTRL+E and from the Options menu, applied at once and
# written to the file.
c=$(card "$proj/bin/ade.bin")
printf '\tx\r\n' > "$c/settings.txt"
under_hub "$c" 40 "ade settings.txt"
played "$c" "settings: changed with CTRL+E, then from Options" "keys 17 of 17
current 1
at 1:3
screen 1:8
window 1 settings.txt 1:3 	x
window 2 closed
window 3 closed"
check "  and written to ade.ini" \
      "$(grep '^tab = ' "$c/config/ade.ini" 2>/dev/null | tr -d '\r')" "tab = 6"

# ...and a font named in ade.ini is ade's on every start: the first, and
# each return from a build, after hub has put the prompt's font back. An
# 8x16 font -- 256 glyphs of 16 rows; blank ones read as well as any -- gives
# mode 0 30 rows, against the system font's 60. The scripted ade reads the
# screen back as the resume asks for its first key, a line a row.
c=$(card "$proj/bin/ade.bin")
mkdir -p "$c/config/aed"
head -c 4096 /dev/zero > "$c/config/aed/tall.bin"
printf '[editor]\r\nfont = /config/aed/tall.bin\r\n' > "$c/config/ade.ini"
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/fontok.c"
under_hub "$c" 120 "ade fontok.c"
check "settings: the font ade.ini names, again after a build" \
      "$(wc -l < "$c/ade.scr" 2>/dev/null | tr -d ' ')" 30

# A menu pulled down is in the bar's colours, but for the item chosen, which
# is in the text's: a bar of them across the box. The scripted ade reads the
# pixels back from the VDP with the File menu down and its second item chosen.
c=$(card "$proj/bin/ade.bin")
printf 'some text\r\n' > "$c/pull.txt"
under_hub "$c" 40 "ade pull.txt"
pull=$(tr -d '\r\n' < "$c/ade.pull" 2>/dev/null)
check "a menu pulled down: its items in the bar's colours" \
      "$(set -- $pull; [ -n "$2" ] && [ "$2" = "$6" ] && [ "$2" != "$8" ] && echo bar)" bar
check "  and the chosen one in the text's" \
      "$(set -- $pull; [ -n "$4" ] && [ "$4" = "$8" ] && echo text)" text

# --- memory ---
# ade at its fullest -- three windows, each on a file too big for its memory,
# so each pages from the card -- and what is left for the heap and the stack,
# which grow toward each other out of one region with no guard between. The
# scripted ade counts it in KB. Measured at 35 with aed 1.3.7 and settings
# (ade 113 KB); the floor is there to say when that is being eaten.
MEMORY_FLOOR_KB=16
c=$(card "$proj/bin/ade.bin")
for f in memory m2 m3; do
    i=0
    while [ $i -lt 2500 ]; do
        printf 'line %05d of a file too big for one window\r\n' $i
        i=$((i + 1))
    done > "$c/$f.txt"
done
under_hub "$c" 60 "ade memory.txt"
free_kb=$(tr -d '\r\n' < "$c/ade.mem" 2>/dev/null)
echo "MEM   three paged windows open, ${free_kb:-?} KB left"
check "three paged windows open, $MEMORY_FLOOR_KB KB or more left" \
      "$([ "${free_kb:-0}" -ge "$MEMORY_FLOOR_KB" ] && echo yes)" yes

# --- under hub: building ---
# F9 on a broken file: hub runs the tool, which fails; ade comes back on the
# error -- its file, line and column -- and shows it, which waits for a key.
if [ ! -f "$ACC_BIN" ] || [ ! -f "$ZAP_BIN" ]; then
    echo "SKIP  building: no Agon acc or zap at $ACC_BIN, $ZAP_BIN"

    exit $status
fi
c=$(card "$proj/bin/ade.bin")
printf 'int main(void)\r\n{\r\n    return x;\r\n}\r\n' > "$c/bad.c"
under_hub "$c" 120 "ade bad.c"
played "$c" "F9 on a broken C file: back on acc's error" "keys 2 of 2
current 1
at 3:12
screen 3:12
window 1 bad.c 3:12 int main(void)
window 2 closed
window 3 closed"
check "  where acc said it was" "$(tr -d '\r' < "$c/ade.err" 2>/dev/null | head -1)" \
      "bad.c:3:12: error: 'x' is not declared"

# An error too long for the bottom row with " (press any key)" after it.
# Written in full it wrapped, and the VDP scrolled the whole screen up a line
# under the message: the menu bar gone and the document a row out. The
# scripted ade reads the screen back from the VDP with the message up; the
# header and the first line must be where they belong.
c=$(card "$proj/bin/ade.bin")
printf '#include <stdio.h>\r\n\r\nint main(void) {\r\n    printf("Hello, world!");\r\n\r\n    return ;\r\n}\r\n' \
    > "$c/bad.c"
under_hub "$c" 120 "ade bad.c"
# The title keeps the top row and the menu bar the one under it, so the
# document's first line is on row 2. The title is drawn centred in a rule, and
# the rule's dashes are what reads back; the bars' own text, in their swapped
# colours, does not.
check "a long error, shown: the screen stays where it is" \
      "$(sed -n '1s/^\(----\).*/\1/p; 3s/ *$//p' "$c/ade.scr" 2>/dev/null | tr '\n' '|')" \
      "----| #include <stdio.h>|"

c=$(card "$proj/bin/ade.bin")
printf '  nop\r\n  frob a\r\n' > "$c/bad.s"
under_hub "$c" 120 "ade bad.s"
played "$c" "F9 on a broken assembly file: back on zap's error" "keys 2 of 2
current 1
at 2:3
screen 2:3
window 1 bad.s 2:3   nop
window 2 closed
window 3 closed"

# The build carries ade's windows across: good.txt changed in window 1, and
# good.c open and changed in window 2, on screen. Both are saved for the tool,
# and ade comes back with both windows, each cursor where it was, and window 2
# on screen.
c=$(card "$proj/bin/ade.bin")
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/good.c"
printf 'bravo\r\n' > "$c/good.txt"
under_hub "$c" 120 "ade good.txt"
played "$c" "F9 on a good C file: built, and back where it was" "keys 2 of 2
current 2
at 3:2
screen 3:2
window 1 good.txt 1:7 bravoy
window 2 good.c 3:2 int main(void)
window 3 closed"
check "  every window was saved first" \
      "$(sed -n 3p "$c/good.c" | tr -d '\r'; tr -d '\r' < "$c/good.txt")" \
      "     return 0;
bravoy"
check "  the program is on the card" "$([ -s "$c/good.bin" ] && echo yes)" yes

# --- under hub: projects ---
# project.ini names the program and its files: F9 builds each and links them,
# whatever is on screen. C and assembly together, with the default rules; hub
# then runs what was built, which says what the two halves worked out.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = prog\r\nfiles = main.c add.c seven.s\r\n' > "$c/project.ini"
printf '#include <stdio.h>\r\nint add(int a, int b);\r\nint seven(void);\r\n' > "$c/main.c"
printf 'int main(void)\r\n{\r\n    FILE* f = fopen("/ran.txt", "w");\r\n' >> "$c/main.c"
printf '    fprintf(f, "%%d\\n", add(seven(), 5));\r\n    fclose(f);\r\n    return 0;\r\n}\r\n' >> "$c/main.c"
printf 'int add(int a, int b)\r\n{\r\n    return a + b;\r\n}\r\n' > "$c/add.c"
printf '        XDEF    _seven\r\n        SEGMENT CODE\r\n_seven:\r\n        ld      hl, 7\r\n        ret\r\n' > "$c/seven.s"
under_hub "$c" 180 "ade main.c
prog"
played "$c" "F9 in a project: every file built, and linked" "keys 2 of 2
current 1
at 1:1
screen 1:1
window 1 main.c 1:1 #include <stdio.h>
window 2 closed
window 3 closed"
check "  and the header names the project's program" "$(header "$c")" "prog  [1]"
check "  one object per file" \
      "$(for o in main.o add.o seven.o; do [ -s "$c/$o" ] && printf '%s ' $o; done)" \
      "main.o add.o seven.o "
check "  and the program, which runs" "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" 12

# An error in a file of the project that is not on screen: ade comes back on
# it, in a window of its own.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = prog\r\nfiles = main.c add.c\r\n' > "$c/project.ini"
printf 'int add(int a, int b);\r\nint main(void)\r\n{\r\n    return add(1, 2);\r\n}\r\n' > "$c/main.c"
printf 'int add(int a, int b)\r\n{\r\n    return a + c;\r\n}\r\n' > "$c/add.c"
under_hub "$c" 180 "ade main.c"
played "$c" "F9 in a project, add.c broken: back on its error" "keys 2 of 2
current 2
at 3:16
screen 3:16
window 1 main.c 1:1 int add(int a, int b);
window 2 add.c 3:16 int add(int a, int b)
window 3 closed"
check "  and nothing was linked" "$([ -e "$c/prog.bin" ] && echo linked || echo no)" no

# The build is build.obey, run by MOS's Obey as one of hub's jobs, so a
# project is not held to hub's eight: here, eight files and the link.
c=$(card "$proj/bin/ade.bin")
files=main.c
: > "$c/main.c"
for n in 1 2 3 4 5 6 7; do
    printf 'int f%d(void)\r\n{\r\n    return %d;\r\n}\r\n' $n $n > "$c/f$n.c"
    printf 'int f%d(void);\r\n' $n >> "$c/main.c"
    files="$files f$n.c"
done
printf '#include <stdio.h>\r\nint main(void)\r\n{\r\n    FILE* f = fopen("/ran.txt", "w");\r\n' >> "$c/main.c"
printf '    fprintf(f, "%%d\\n", f1() + f2() + f3() + f4() + f5() + f6() + f7());\r\n' >> "$c/main.c"
printf '    fclose(f);\r\n    return 0;\r\n}\r\n' >> "$c/main.c"
printf '[project]\r\nname = prog\r\nfiles = %s\r\n' "$files" > "$c/project.ini"
under_hub "$c" 240 "ade main.c
prog"
check "F9 in a project of eight files: built, linked and run" \
      "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" 28
check "  from build.obey, which ade wrote" \
      "$(head -1 "$c/build.obey" 2>/dev/null | cut -c1-30)" "# Made by ade from project.ini"

# build.obey is written again only when project.ini changes: a line added to
# it by hand -- a copy, which leaves a file to see -- lasts through a build
# with project.ini as it was...
printf 'copy main.c kept.txt\r\n' >> "$c/build.obey"
under_hub "$c" 240 "ade main.c"
check "  a hand edit to build.obey lasts while project.ini is unchanged" \
      "$([ -e "$c/kept.txt" ] && echo kept || echo gone)" kept

# ...and is gone once a file is added to the project, which is built.
printf 'int f8(void)\r\n{\r\n    return 8;\r\n}\r\n' > "$c/f8.c"
printf '[project]\r\nname = prog\r\nfiles = %s f8.c\r\n' "$files" > "$c/project.ini"
rm -f "$c/kept.txt"
under_hub "$c" 240 "ade main.c"
check "  a file added to project.ini writes it again" \
      "$([ -s "$c/f8.o" ] && echo built || echo not)$([ -e "$c/kept.txt" ] && echo ' kept')" built

# A project.ini that names a file nothing builds: F9 says so and builds none.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = prog\r\nfiles = wrong.c notes.txt\r\n' > "$c/project.ini"
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/wrong.c"
printf 'notes\r\n' > "$c/notes.txt"
under_hub "$c" 60 "ade wrong.c"
played "$c" "F9 in a project that cannot be built: says why, and stays" "keys 3 of 3
current 1
at 1:1
screen 1:1
window 1 wrong.c 1:1 int main(void)
window 2 closed
window 3 closed"
check "  and built nothing" "$([ -e "$c/wrong.o" ] && echo built || echo no)" no

# A project's own rules: an assembly program in one piece, zap writing the
# binary itself (%n), and no link.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = hello\r\nfiles = hello.s\r\n[build]\r\ns = zap %%f %%n.bin -c -e %%e\r\nlink =\r\n' > "$c/project.ini"
printf '        ret\r\n' > "$c/hello.s"
under_hub "$c" 120 "ade hello.s"
played "$c" "F9 in a project with its own rules" "keys 2 of 2
current 1
at 1:1
screen 1:1
window 1 hello.s 1:1         ret
window 2 closed
window 3 closed"
check "  zap wrote the program itself" "$([ -s "$c/hello.bin" ] && echo yes)" yes

# --- under hub: more than one error ---
# A tool that reports three errors -- test/tools/fail.c, standing in for acc
# and zap, which stop at one -- as a project's rule. ade comes back on the
# first with the list of them up; the script goes to the third, in a file not
# yet open, then brings the list back with ALT+F9 and goes to the second.
if [ -x "$ACC_DIR/bin/acc" ]; then
    "$ACC_DIR/bin/acc" test/tools/fail.c "$ACC_DIR/bin/libc.a" -I "$ACC_DIR/include" \
        -o "$W/fail.bin" >/dev/null 2>&1
fi
if [ -s "$W/fail.bin" ]; then
    c=$(card "$proj/bin/ade.bin")
    cp "$W/fail.bin" "$c/bin/fail.bin"
    : > "$c/errlist"
    printf '[project]\r\nname = prog\r\nfiles = main.c add.c\r\n[build]\r\nc = fail %%e\r\n' \
        > "$c/project.ini"
    printf 'int add(int a, int b);\r\nint main(void)\r\n{\r\n    return add(1, 2);\r\n}\r\n' \
        > "$c/main.c"
    printf 'int add(int a, int b)\r\n{\r\n    return a + b;\r\n}\r\n' > "$c/add.c"
    under_hub "$c" 120 "ade main.c"
    played "$c" "three errors: the list, to the third, and back to the second" "keys 9 of 9
current 1
at 4:1
screen 4:1
window 1 main.c 4:1 int add(int a, int b);
window 2 add.c 1:3 int add(int a, int b)
window 3 closed"
    check "  and the header shows add.c's window beside it" "$(header "$c")" "prog  [1] 2"
else
    echo "SKIP  more than one error: no host acc at $ACC_DIR/bin/acc to build test/tools/fail.c"
fi

# --- under hub: running ---
# CTRL+F9 builds, then hub runs the program -- on its own screen, and held for
# a key after -- then ade. Hub$NoPause lets a script go on past that key.
RUNME='#include <stdio.h>\r\nint main(void)\r\n{\r\n    FILE* f = fopen("/ran.txt", "w");\r\n    fputs("ran\\n", f);\r\n    fclose(f);\r\n    return 3;\r\n}\r\n'
c=$(card "$proj/bin/ade.bin")
printf "$RUNME" > "$c/runme.c"
under_hub "$c" 120 'Set Hub$NoPause 1
ade runme.c'
played "$c" "CTRL+F9: built, run, and back" "keys 2 of 2
current 1
at 1:1
screen 1:1
window 1 runme.c 1:1 #include <stdio.h>
window 2 closed
window 3 closed"
check "  the program ran" "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" ran

# Without Hub$NoPause hub waits for a key once the program ends, so the
# program has run and ade is not back: what ade last wrote is from before.
c=$(card "$proj/bin/ade.bin")
printf "$RUNME" > "$c/runme.c"
under_hub "$c" 60 "ade runme.c"
check "CTRL+F9, pausing: the program ran" "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" ran
check "  and hub waits for a key before ade" \
      "$(tr -d '\r' < "$c/ade.out" 2>/dev/null | head -1)" "keys 1 of 1"

# A build that fails runs nothing: ade is back on the error, and a program
# left by an earlier build -- one that would say it ran -- stays unrun.
c=$(card "$proj/bin/ade.bin")
printf 'int main(void)\r\n{\r\n    return x;\r\n}\r\n' > "$c/runbad.c"
printf "$RUNME" > "$c/runme.c"
under_hub "$c" 120 'Set Hub$NoPause 1
ade runme.c'
cp "$c/runme.bin" "$c/runbad.bin" 2>/dev/null
rm -f "$c/ran.txt" "$c/ade.out"
under_hub "$c" 120 'Set Hub$NoPause 1
ade runbad.c'
played "$c" "CTRL+F9 on a broken file: back on the error" "keys 2 of 2
current 1
at 3:12
screen 3:12
window 1 runbad.c 3:12 int main(void)
window 2 closed
window 3 closed"
check "  and the program was not run" "$([ -e "$c/ran.txt" ] && echo ran || echo no)" no

# ALT+F5: what the program left on the screen. hub captured it as the
# program ended; ade shows it until a key, then itself again. The program
# clears the screen to red, and the script reads a pixel back from the VDP --
# the full emulator's, so a real one -- with each screen up. The report ends
# with the mode as ade's main began and as it finished, and the mode hub
# captured in.
#
# user_screen <card> <what> <mode or empty>: build and run the red program --
# first switching to <mode>, if given -- then ALT+F5 and back.
user_screen() {
    local c=$1 what=$2 mode=$3 px user rest
    : > "$c/userscreen"
    printf '#include <stdio.h>\r\nint main(void)\r\n{\r\n' > "$c/runshow.c"
    [ -n "$mode" ] && printf '    putchar(22);\r\n    putchar(%d);\r\n' "$mode" >> "$c/runshow.c"
    printf '    putchar(17);\r\n    putchar(129);\r\n    putchar(12);\r\n    return 0;\r\n}\r\n' \
        >> "$c/runshow.c"
    under_hub "$c" 150 'Set Hub$NoPause 1
ade runshow.c'
    px=$(tr -d '\r\n' < "$c/ade.px" 2>/dev/null)
    echo "PIX   $px"
    user=${px#user }
    user=${user%% *}
    rest=${px#* ade }
    check "$what" \
          "$(case "$user" in ??0000) [ "$user" != 000000 ] && echo red ;; *) echo "$user" ;; esac)" red
    check "  and then ade's own again" "$([ -n "$px" ] && [ "${rest%% *}" != "$user" ] && echo back)" back
}

user_screen "$(card "$proj/bin/ade.bin")" "ALT+F5: the program's screen, red, as it left it" ""

# A program that changes the mode: ade comes back from it in its own mode,
# mode 0, and ALT+F5 shows the capture in the program's, mode 3, then
# returns to ade's.
c=$(card "$proj/bin/ade.bin")
user_screen "$c" "ALT+F5 after a program in mode 3: its screen, red" 3
check "  hub captured it in mode 3, and ade ends in its own, mode 0" \
      "$(tr -d '\r\n' < "$c/ade.px" 2>/dev/null | sed 's/.* mode [0-9]* //')" "0 captured 3"

# A project's run line: how Run runs the program, arguments and all.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = runargs\r\nfiles = runargs.c\r\nrun = %%n alpha beta\r\n' > "$c/project.ini"
printf '#include <stdio.h>\r\nint main(int argc, char** argv)\r\n{\r\n    FILE* f = fopen("/ran.txt", "w");\r\n' > "$c/runargs.c"
printf '    fprintf(f, "%%d %%s\\n", argc, argc > 2 ? argv[2] : "-");\r\n    fclose(f);\r\n    return 0;\r\n}\r\n' >> "$c/runargs.c"
under_hub "$c" 120 'Set Hub$NoPause 1
ade runargs.c'
check "CTRL+F9 in a project: run with the arguments project.ini gives" \
      "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" "3 beta"

# --- how long ade takes to start, at the Agon's own speed ---
# Every build restarts ade, so its start-up is part of every build's wait.
# The scripted ade times itself, in hundredths, from main to its first key:
# the screen drawn -- or, resumed, the outcome shown. The good build again,
# unhurried; MOS loading the binary is not in it, since the emulator reads
# the card from the host's disk.
#
# Measured: 42 to start. A resume is the start, 20 to open window 2, and 20
# for each full paint. Two windows came back in 160 when every window was
# shown and placed and the target shown again, and in 140 when the target was
# placed last, once; placing each without drawing (ed_doc_place, aed 1.3.7)
# and painting only the target brought it to 84.
RESUME_CS=100
c=$(card "$proj/bin/ade.bin")
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/good.c"
printf 'bravo\r\n' > "$c/good.txt"
under_hub "$c" 400 "ade good.txt" paced
start=$(tr -d '\r\n' < "$c/ade.time" 2>/dev/null)
resume=$(tr -d '\r\n' < "$c/ade-resume.time" 2>/dev/null)
echo "TIME  start-up ${start:-?} cs, resumed with two windows ${resume:-?} cs"
check "ade starts in under 0.6 s" "$([ "${start:-999}" -lt 60 ] && echo yes)" yes
check "  and comes back from a build in under $RESUME_CS cs" \
      "$([ "${resume:-999}" -lt "$RESUME_CS" ] && echo yes)" yes

exit $status
