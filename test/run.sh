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
# running <command> from a script, as a person would at hub's prompt; the
# emulator stops when the script is done, or after <seconds> if it never gets
# there. Whatever ade wrote is on the card. Run flat out
# (-u) -- acc is a quarter of a megabyte to load and run -- unless "paced":
# then at the Agon's own speed, for timing.
under_hub() {
    local c=$1 secs=$2 fifo hold speed=-u
    [ "${4:-}" = paced ] && speed=
    # Then the emulator's own way out, from MOS's directory on the card: run
    # once the commands are done, it stops the machine there and then, where
    # before every case sat at the prompt until its timeout -- the whole of
    # it, however soon ade had finished. A case that never gets this far,
    # such as one where ade is meant still to be running, still times out.
    printf '%s\nemulator_exit_success\n' "$3" > "$c/script.txt"
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

# plant, from test/tools: a session left in hub's block for ade -resume to
# find, as ade's own or as another layout's -- built the same way, against
# ade's session.h.
plant="$W/plant"
mkdir -p "$plant/src"
cp test/tools/plant.c "$plant/src/"
cat > "$plant/Makefile" <<MK
NAME=plant
include \$(shell agondev-config --makefile)
CFLAGS += -I$PWD/src -I$LIBS_ABS/include/core -I$LIBS_ABS/include/ui -I$HUB/include
LIBS := $HUB/build/lib/agondev/libhub.a
MK
(cd "$plant" && make >/dev/null 2>&1)
check "plant builds" "$([ -f "$plant/bin/plant.bin" ] && echo built)" built

# zero, from test/tools: a program that returns 0, to stand on a card as
# /bin/shell.bin. And hub 0.5.0, from its tag in the hub checkout, the last
# without HUB_SHELL: what ade's shell meets on an older hub.
zero="$W/zero"
mkdir -p "$zero/src"
cp test/tools/zero.c "$zero/src/"
printf 'NAME=zero\ninclude $(shell agondev-config --makefile)\n' > "$zero/Makefile"
(cd "$zero" && make >/dev/null 2>&1)
check "zero builds" "$([ -f "$zero/bin/zero.bin" ] && echo built)" built
OLDHUB=
if git -C "$HUB" rev-parse -q --verify v0.5.0 >/dev/null; then
    mkdir -p "$W/hub050"
    git -C "$HUB" archive v0.5.0 | tar -x -C "$W/hub050"
    (cd "$W/hub050" && make >/dev/null 2>&1)
    [ -f "$W/hub050/build/hub.bin" ] && OLDHUB="$W/hub050/build/hub.bin"
fi
check "hub 0.5.0 builds, for the shell on an older hub" "${OLDHUB:+built}" built

# strip <file>: the end of the menu bar's row, past the menus' names, as read.
strip() {
    sed -n '2p' "$1" 2>/dev/null | tr '?' ' ' | sed 's/ *$//' | awk -F'   +' '{print $NF}'
}

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

# Shared by the cases below, which each run in a shell of their own: the
# memory left that is enough (see the memory case), and the program the Run
# cases build and run.
MEMORY_FLOOR_KB=32
RUNME='#include <stdio.h>\r\nint main(void)\r\n{\r\n    FILE* f = fopen("/ran.txt", "w");\r\n    fputs("ran\\n", f);\r\n    fclose(f);\r\n    return 3;\r\n}\r\n'

# Building needs acc and zap on the card; a case that builds runs only with
# both.
TOOLS=yes
if [ ! -f "$ACC_BIN" ] || [ ! -f "$ZAP_BIN" ]; then
    echo "SKIP  building: no Agon acc or zap at $ACC_BIN, $ZAP_BIN"
    TOOLS=
fi

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

case_01() {
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
}

case_01b() {
# A second window, opened with CTRL+SHIFT+O and left with F6: the bar's list of windows
# read back from the screen each time. It was drawn as the new window cleared
# the screen, before ade had counted it, so it still said [1] until F6.
c=$(card "$proj/bin/ade.bin")
printf 'first file\r\n' > "$c/one.txt"
printf '#define FUNC 1\r\n' > "$c/func.h"
under_hub "$c" 40 "ade one.txt"
check "CTRL+SHIFT+O to a second window: the bar shows both, 2 on screen" "$(strip "$c/ade.scr")" "1 [2]"
check "  and CTRL+TAB back to 1: the bar shows 1 on screen" "$(strip "$c/ade.scr2")" "[1] 2"
# The footer names the file on screen: it went on naming the first window's,
# drawn from the document the editor's loop started with, until aed 1.5.1.
check "  the footer names func.h in window 2" "$(grep '^footer ' "$c/ade.scr" 2>/dev/null)" "footer func.h"
check "  under the title" "$(grep '^title ' "$c/ade.scr" 2>/dev/null)" \
      "title ADE: Another Development Environment"
check "  and one.txt back in window 1" "$(grep '^footer ' "$c/ade.scr2" 2>/dev/null)" "footer one.txt"
}

case_01c() {
# Any window but the last can be closed, window 1 included, and the ones
# after it move down a number: with three open, closing window 1 makes b.txt
# window 1 and c.txt window 2. It used to be refused: "Window 1 stays open".
c=$(card "$proj/bin/ade.bin")
printf 'first\r\n' > "$c/close1.txt"
printf 'bravo\r\n' > "$c/b.txt"
printf 'charlie\r\n' > "$c/c.txt"
under_hub "$c" 40 "ade close1.txt"
check "closing window 1: the others move down, b.txt window 1 of 2" \
      "$(strip "$c/ade.scr")" "[1] 2"
check "  and it is b.txt on screen" "$(grep '^footer ' "$c/ade.scr" 2>/dev/null)" "footer b.txt"
played "$c" "  closing window 2 too leaves b.txt, and the last stays open" "keys 21 of 21
current 1
at 1:1
screen 1:1
window 1 b.txt 1:1 bravo
window 2 closed
window 3 closed"
}

case_01d() {
# Round the windows by keys: CTRL+TAB to the next, CTRL+SHIFT+TAB to the one
# before, CTRL+number to that one, and nothing for a window that is not
# there. The window on screen as each key is asked for, from 3 after opening
# them: 3, then 1, 3, 2, 1, 3, 2 after closing 3, 2 still, and 2 and 2 for
# F6 and F9, which were next window and build and are nothing now -- F9
# building would wait on "Build what?" and never get to leaving.
c=$(card "$proj/bin/ade.bin")
printf 'nav\r\n' > "$c/nav.txt"
printf 'bravo\r\n' > "$c/b.txt"
printf 'charlie\r\n' > "$c/c.txt"
under_hub "$c" 40 "ade nav.txt"
check "CTRL+TAB, CTRL+SHIFT+TAB and CTRL+number go round the windows" \
      "$(cat "$c/ade.nav" 2>/dev/null)" 3132132222
}

case_01e() {
# More windows than can be loaded: three at a time, the rest put away. Three
# changed and unsaved, opening a fourth asks to save the one used longest ago
# -- no opens nothing; yes saves it and puts it away. CTRL+1 then loads it
# back, putting away the one that is saved; CTRL+0 finds no window 10.
c=$(card "$proj/bin/ade.bin")
printf 'one\r\n' > "$c/w1.txt"
printf 'two\r\n' > "$c/w2.txt"
printf 'three\r\n' > "$c/w3.txt"
printf 'four\r\n' > "$c/w4.txt"
under_hub "$c" 60 "ade w1.txt"
played "$c" "four windows, three loaded: w4.txt put away for w1.txt" "keys 42 of 42
current 3
at 1:2
screen 1:2
window 1 w1.txt 1:2 Aone
window 2 w2.txt 1:2 Btwo
window 3 w3.txt 1:2 Cthree
window 4 w4.txt 1:1 (put away)"
check "  w1.txt saved to make room, w2 and w3 left as they were" \
      "$(cat "$c/w1.txt" "$c/w2.txt" "$c/w3.txt" "$c/w4.txt" | tr -d '\r' | tr '\n' ' ')" \
      "Aone two three four "
check "  and the bar shows four windows, 3 on screen" "$(header "$c")" "1 2 [3] 4"
}

case_01f() {
# A window keeps its file's name to load it from again only up to 63
# characters: one with a longer path stays loaded. Window 1's path is 69
# long, so making room for w4.txt puts away w2.txt, though window 1 is the
# one used longest ago.
c=$(card "$proj/bin/ade.bin")
mkdir -p "$c/a_directory_with_a_long_name/another_directory_long/third_one"
printf 'one\r\n' > "$c/a_directory_with_a_long_name/another_directory_long/third_one/w1.txt"
printf 'two\r\n' > "$c/w2.txt"
printf 'three\r\n' > "$c/w3.txt"
printf 'four\r\n' > "$c/w4.txt"
under_hub "$c" 60 "ade /a_directory_with_a_long_name/another_directory_long/third_one/w1.txt"
played "$c" "a long path stays loaded: w2.txt put away instead" "keys 25 of 25
current 4
at 1:1
screen 1:1
window 1 /a_directory_with_a_long_name/another_directory_long/third_one/w1.txt 1:1 one
window 2 w2.txt 1:1 (put away)
window 3 w3.txt 1:1 three
window 4 w4.txt 1:1 four"
}

case_01g() {
# Nine windows at most: f1.txt to f9.txt open, three of them loaded -- the
# last three, the earlier put away in turn -- and a tenth refused.
c=$(card "$proj/bin/ade.bin")
for n in 1 2 3 4 5 6 7 8 9; do
    printf 'f%d\r\n' $n > "$c/f$n.txt"
done
under_hub "$c" 60 "ade f1.txt"
played "$c" "nine windows open, the tenth refused" "keys 67 of 67
current 9
at 1:1
screen 1:1
window 1 f1.txt 1:1 (put away)
window 2 f2.txt 1:1 (put away)
window 3 f3.txt 1:1 (put away)
window 4 f4.txt 1:1 (put away)
window 5 f5.txt 1:1 (put away)
window 6 f6.txt 1:1 (put away)
window 7 f7.txt 1:1 f7
window 8 f8.txt 1:1 f8
window 9 f9.txt 1:1 f9"
}

case_02() {
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
}

case_03() {
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
}

case_04() {
    [ -n "$TOOLS" ] || return 0
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
      "$(grep -vc '^footer \|^title \|^said ' "$c/ade.scr" 2>/dev/null)" 30
}

case_05() {
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

}

case_06() {
# --- memory ---
# ade at its fullest -- three windows, each on a file too big for its memory,
# so each pages from the card -- and what is left for the heap and the stack,
# which grow toward each other out of one region with no guard between. The
# scripted ade counts it in KB. It was 35 with 48 KB windows, then 16 once
# nine windows came in; with 40 KB windows, 24 KB more. The floor is there to
# say when that is being eaten.
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
}

case_07() {
    [ -n "$TOOLS" ] || return 0
# --- under hub: building ---
# F9 on a broken file: hub runs the tool, which fails; ade comes back on the
# error -- its file, line and column -- and shows it, which waits for a key.
c=$(card "$proj/bin/ade.bin")
printf 'int main(void)\r\n{\r\n    return x;\r\n}\r\n' > "$c/bad.c"
under_hub "$c" 120 "ade bad.c"
played "$c" "CTRL+B on a broken C file: back on acc's error" "keys 2 of 2
current 1
at 3:12
screen 3:12
window 1 bad.c 3:12 int main(void)
window 2 closed
window 3 closed"
check "  where acc said it was" "$(tr -d '\r' < "$c/ade.err" 2>/dev/null | head -1)" \
      "bad.c:3:12: error: 'x' is not declared"
check "  and says so from its line and column, the file in the footer" \
      "$(grep '^said ' "$c/ade.scr" 2>/dev/null | tr -d '\r')" \
      "said 3:12: error: 'x' is not declared"
}

case_08() {
    [ -n "$TOOLS" ] || return 0
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
}

case_08b() {
    [ -n "$TOOLS" ] || return 0
# A function called and defined nowhere: acc finds it as it links, where
# there is no line to point at, and writes it as at acc:0:0. It is shown as
# acc wrote it, the cursor left where it was; it used to be dropped, for
# "The build failed, with 100".
c=$(card "$proj/bin/ade.bin")
printf '#include "func.h"\r\n\r\nint main(void)\r\n{\r\n    return add(2, 3);\r\n}\r\n' > "$c/call.c"
printf 'int add(int a, int b);\r\n' > "$c/func.h"
under_hub "$c" 120 "ade call.c"
check "an error with no place in a file: shown as acc wrote it" \
      "$(grep '^said ' "$c/ade.scr" 2>/dev/null | tr -d '\r')" \
      "said error: 'add' is called but never defined"
}

case_09() {
    [ -n "$TOOLS" ] || return 0
c=$(card "$proj/bin/ade.bin")
printf '  nop\r\n  frob a\r\n' > "$c/bad.s"
under_hub "$c" 120 "ade bad.s"
played "$c" "CTRL+B on a broken assembly file: back on zap's error" "keys 2 of 2
current 1
at 2:3
screen 2:3
window 1 bad.s 2:3   nop
window 2 closed
window 3 closed"
}

case_09b() {
    [ -n "$TOOLS" ] || return 0
# A build that works comes straight back to the editor: the bar says built,
# and nothing waits for a key -- the resume is given only CTRL+Q, which a
# message waiting would take for itself, and ade would not leave.
c=$(card "$proj/bin/ade.bin")
: > "$c/nowait"
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/okay.c"
under_hub "$c" 120 "ade okay.c"
# The bar as the resumed ade left it: a resume stuck on a message never gets
# that far, and what is there is the build's run, before it said anything.
check "a build that works: no key to press, and the bar says built" \
      "$(header "$c")" "built  [1]"
}

case_10() {
    [ -n "$TOOLS" ] || return 0
# The build carries ade's windows across: good.txt changed in window 1, and
# good.c open and changed in window 2, on screen. Both are saved for the tool,
# and ade comes back with both windows, each cursor where it was, and window 2
# on screen.
c=$(card "$proj/bin/ade.bin")
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/good.c"
printf 'bravo\r\n' > "$c/good.txt"
under_hub "$c" 120 "ade good.txt"
played "$c" "CTRL+B on a good C file: built, and back where it was" "keys 2 of 2
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
}

case_11() {
    [ -n "$TOOLS" ] || return 0
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
played "$c" "CTRL+B in a project: every file built, and linked" "keys 2 of 2
current 1
at 1:1
screen 1:1
window 1 main.c 1:1 #include <stdio.h>
window 2 closed
window 3 closed"
check "  and the bar says it built, and names the project's program" "$(header "$c")" "built  prog  [1]"
check "  one object per file" \
      "$(for o in main.o add.o seven.o; do [ -s "$c/build/$o" ] && printf '%s ' $o; done)" \
      "main.o add.o seven.o "
check "  and the program, which runs" "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" 12
}

case_12() {
    [ -n "$TOOLS" ] || return 0
# An error in a file of the project that is not on screen: ade comes back on
# it, in a window of its own.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = prog\r\nfiles = main.c add.c\r\n' > "$c/project.ini"
printf 'int add(int a, int b);\r\nint main(void)\r\n{\r\n    return add(1, 2);\r\n}\r\n' > "$c/main.c"
printf 'int add(int a, int b)\r\n{\r\n    return a + c;\r\n}\r\n' > "$c/add.c"
under_hub "$c" 180 "ade main.c"
played "$c" "CTRL+B in a project, add.c broken: back on its error" "keys 2 of 2
current 2
at 3:16
screen 3:16
window 1 main.c 1:1 int add(int a, int b);
window 2 add.c 3:16 int add(int a, int b)
window 3 closed"
check "  and nothing was linked" "$([ -e "$c/bin/prog.bin" ] && echo linked || echo no)" no
}

case_13() {
    [ -n "$TOOLS" ] || return 0
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
check "CTRL+B in a project of eight files: built, linked and run" \
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
      "$([ -s "$c/build/f8.o" ] && echo built || echo not)$([ -e "$c/kept.txt" ] && echo ' kept')" built
}

case_14() {
    [ -n "$TOOLS" ] || return 0
# A project.ini that names a file nothing builds: F9 says so and builds none.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = prog\r\nfiles = wrong.c notes.txt\r\n' > "$c/project.ini"
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/wrong.c"
printf 'notes\r\n' > "$c/notes.txt"
under_hub "$c" 60 "ade wrong.c"
played "$c" "CTRL+B in a project that cannot be built: says why, and stays" "keys 3 of 3
current 1
at 1:1
screen 1:1
window 1 wrong.c 1:1 int main(void)
window 2 closed
window 3 closed"
check "  and built nothing" "$([ -e "$c/wrong.o" ] && echo built || echo no)" no
}

case_15a() {
    [ -n "$TOOLS" ] || return 0
# Build -> New project writes a project.ini with no list of files, so the
# project is every .c, .s and .asm in the directory: newproj.c calling add()
# in func.c is built, into build/ and bin/, linked and run. In a directory of
# its own, as a project would be, whose bin/ is not the card's.
c=$(card "$proj/bin/ade.bin")
mkdir -p "$c/p"
d=$c/p
printf '#include <stdio.h>\r\n#include "func.h"\r\n\r\nint main(void)\r\n{\r\n' > "$d/newproj.c"
printf '    FILE* f = fopen("/ran.txt", "w");\r\n    fprintf(f, "%%d\\n", add(2, 3));\r\n' >> "$d/newproj.c"
printf '    fclose(f);\r\n    return 0;\r\n}\r\n' >> "$d/newproj.c"
printf 'int add(int a, int b);\r\n' > "$d/func.h"
printf 'int add(int a, int b)\r\n{\r\n    return a + b;\r\n}\r\n' > "$d/func.c"
under_hub "$c" 180 "cd /p
ade newproj.c
bin/newproj"
check "Build -> New project: project.ini, naming newproj" \
      "$(tr -d '\r' < "$d/project.ini" 2>/dev/null | grep '^name')" "name = newproj"
check "  with no files line: the directory is the project" \
      "$(tr -d '\r' < "$d/project.ini" 2>/dev/null | grep -c '^files')" 0
check "  so CTRL+B builds every file here, func.c with newproj.c" \
      "$(tr -d '\r' < "$d/build.obey" 2>/dev/null | grep -c '^acc -c \(func\|newproj\)\.c')" 2
check "  and the program, linked from both, runs" "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" 5
check "  its objects in build/, the program in bin/, none beside the source" \
      "$(cd "$d" && ls build/func.o build/newproj.o bin/newproj.bin *.o 2>/dev/null | tr '\n' ' ')" \
      "bin/newproj.bin build/func.o build/newproj.o "

# A file added to the directory, project.ini as it was, is in the next build:
# the names found go into build.obey's checksum.
printf 'int extra(void)\r\n{\r\n    return 1;\r\n}\r\n' > "$d/extra.c"
under_hub "$c" 180 "cd /p
ade newproj.c"
check "  a file added to the directory is built next time" \
      "$([ -s "$d/build/extra.o" ] && echo built || echo not)" built

# New project, asked again, leaves the project.ini there as it was -- a line
# added to it by hand included.
printf '; mine\r\n' >> "$d/project.ini"
cp "$d/project.ini" "$d/project.was"
under_hub "$c" 180 "cd /p
ade newproj.c"
check "  and New project leaves the project.ini there alone" \
      "$(cmp -s "$d/project.ini" "$d/project.was" && echo same || echo changed)" same
}

case_15b() {
    [ -n "$TOOLS" ] || return 0
# exclude = leaves a file of the directory out of the project.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = ex\r\nexclude = scratch.c\r\n' > "$c/project.ini"
printf 'int main(void)\r\n{\r\n    return 0;\r\n}\r\n' > "$c/ex.c"
printf 'this is not C\r\n' > "$c/scratch.c"
under_hub "$c" 120 "ade ex.c"
check "a project excluding scratch.c builds without it" \
      "$([ -s "$c/bin/ex.bin" ] && [ ! -e "$c/build/scratch.o" ] && echo built)" built
}

case_15() {
    [ -n "$TOOLS" ] || return 0
# A project's own rules: an assembly program in one piece, zap writing the
# binary itself (%n), and no link.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = hello\r\nfiles = hello.s\r\n[build]\r\ns = zap %%f %%n.bin -c -e %%e\r\nlink =\r\n' > "$c/project.ini"
printf '        ret\r\n' > "$c/hello.s"
under_hub "$c" 120 "ade hello.s"
played "$c" "CTRL+B in a project with its own rules" "keys 2 of 2
current 1
at 1:1
screen 1:1
window 1 hello.s 1:1         ret
window 2 closed
window 3 closed"
check "  zap wrote the program itself" "$([ -s "$c/hello.bin" ] && echo yes)" yes
}

case_16() {
    [ -n "$TOOLS" ] || return 0
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
    check "  and the bar says it failed, with add.c's window" "$(header "$c")" "failed  prog  [1] 2"
else
    echo "SKIP  more than one error: no host acc at $ACC_DIR/bin/acc to build test/tools/fail.c"
fi

# --- under hub: running ---
# CTRL+F9 builds, then hub runs the program -- on its own screen, and held for
# a key after -- then ade. Hub$NoPause lets a script go on past that key.
}

case_17() {
    [ -n "$TOOLS" ] || return 0
c=$(card "$proj/bin/ade.bin")
printf "$RUNME" > "$c/runme.c"
under_hub "$c" 120 'Set Hub$NoPause 1
ade runme.c'
played "$c" "CTRL+R: built, run, and back" "keys 2 of 2
current 1
at 1:1
screen 1:1
window 1 runme.c 1:1 #include <stdio.h>
window 2 closed
window 3 closed"
check "  the program ran" "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" ran
}

case_18() {
    [ -n "$TOOLS" ] || return 0
# Without Hub$NoPause hub waits for a key once the program ends, so the
# program has run and ade is not back: what ade last wrote is from before.
c=$(card "$proj/bin/ade.bin")
printf "$RUNME" > "$c/runme.c"
under_hub "$c" 60 "ade runme.c"
check "CTRL+R, pausing: the program ran" "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" ran
check "  and hub waits for a key before ade" \
      "$(tr -d '\r' < "$c/ade.out" 2>/dev/null | head -1)" "keys 1 of 1"
}

case_19() {
    [ -n "$TOOLS" ] || return 0
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
played "$c" "CTRL+R on a broken file: back on the error" "keys 2 of 2
current 1
at 3:12
screen 3:12
window 1 runbad.c 3:12 int main(void)
window 2 closed
window 3 closed"
check "  and the program was not run" "$([ -e "$c/ran.txt" ] && echo ran || echo no)" no
}

case_20() {
    [ -n "$TOOLS" ] || return 0
# ALT+F5: what the program left on the screen. hub captured it as the
# program ended; ade shows it until a key, then itself again. The program
# clears the screen to red, and the script reads a pixel back from the VDP --
# the full emulator's, so a real one -- with each screen up. The report ends
# with the mode as ade's main began and as it finished, and the mode hub
# captured in.
#


user_screen "$(card "$proj/bin/ade.bin")" "ALT+F5: the program's screen, red, as it left it" ""
}

case_21() {
    [ -n "$TOOLS" ] || return 0
# A program that changes the mode: ade comes back from it in its own mode,
# mode 0, and ALT+F5 shows the capture in the program's, mode 3, then
# returns to ade's.
c=$(card "$proj/bin/ade.bin")
user_screen "$c" "ALT+F5 after a program in mode 3: its screen, red" 3
check "  hub captured it in mode 3, and ade ends in its own, mode 0" \
      "$(tr -d '\r\n' < "$c/ade.px" 2>/dev/null | sed 's/.* mode [0-9]* //')" "0 captured 3"
# Each mode change ade made coming back had finished -- MOS had the VDP's
# answer, and the new mode -- before ade laid the screen out for it. On
# hardware the answer comes well after the VDU 22, and a layout read sooner
# is the old mode's; the emulator answers within a frame or so, but not
# before the next instruction.
check "  and ade waited for its mode to be back before laying it out" \
      "$(tr -d '\r\n' < "$c/ade.mode" 2>/dev/null)" "settled y"
}

case_22() {
    [ -n "$TOOLS" ] || return 0
# A project's run line: how Run runs the program, arguments and all.
c=$(card "$proj/bin/ade.bin")
printf '[project]\r\nname = runargs\r\nfiles = runargs.c\r\nrun = bin/%%n alpha beta\r\n' > "$c/project.ini"
printf '#include <stdio.h>\r\nint main(int argc, char** argv)\r\n{\r\n    FILE* f = fopen("/ran.txt", "w");\r\n' > "$c/runargs.c"
printf '    fprintf(f, "%%d %%s\\n", argc, argc > 2 ? argv[2] : "-");\r\n    fclose(f);\r\n    return 0;\r\n}\r\n' >> "$c/runargs.c"
under_hub "$c" 120 'Set Hub$NoPause 1
ade runargs.c'
check "CTRL+R in a project: run with the arguments project.ini gives" \
      "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" "3 beta"
}

# A session in hub's block that another ade left -- the same MAGIC, another
# layout -- is not taken for this one's: hub keeps a block between ades and
# grows it for a bigger one, keeping its bytes. ade -resume then starts with
# no file. Its own session, planted the same way, is resumed.
case_23() {
local c
for how in own other; do
    c=$(card "$proj/bin/ade.bin")
    cp "$plant/bin/plant.bin" "$c/bin/"
    printf 'planted\r\n' > "$c/planted.txt"
    under_hub "$c" 60 "plant $how
ade -resume"
    eval "foot_$how=\$(grep '^footer ' \"\$c/ade.scr\" 2>/dev/null)"
done
check "ade -resume takes its own session" "$foot_own" "footer planted.txt"
check "  and not one of another layout" "$foot_other" "footer [NO FILE]"
}

# A project as big as ade takes, 32 files with long names: a link line
# naming every object would be 590 characters, and MOS cuts a command at
# 255. The link reads them from build/prog.lnk with acc's @ instead.
case_24() {
    [ -n "$TOOLS" ] || return 0
local c n
c=$(card "$proj/bin/ade.bin")
: > "$c/main.c"
for n in $(seq -w 1 31); do
    printf 'int module_%s(void)\r\n{\r\n    return %d;\r\n}\r\n' $n $((10#$n)) > "$c/module_$n.c"
    printf 'int module_%s(void);\r\n' $n >> "$c/main.c"
done
printf '#include <stdio.h>\r\nint main(void)\r\n{\r\n    int sum = 0;\r\n    FILE* f;\r\n' >> "$c/main.c"
for n in $(seq -w 1 31); do
    printf '    sum += module_%s();\r\n' $n >> "$c/main.c"
done
printf '    f = fopen("/ran.txt", "w");\r\n    fprintf(f, "%%d\\n", sum);\r\n    fclose(f);\r\n    return 0;\r\n}\r\n' >> "$c/main.c"
printf '[project]\r\nname = prog\r\n' > "$c/project.ini"
under_hub "$c" 600 "ade main.c
prog"
check "CTRL+B in a project of 32 files: built, linked and run" \
      "$(tr -d '\r\n' < "$c/ran.txt" 2>/dev/null)" 496
check "  linked from build/prog.lnk, an object a line" \
      "$(tr -d '\r' < "$c/build/prog.lnk" 2>/dev/null | grep -c '^build/.*\.o$')" 32
check "  and no line of build.obey past MOS's 255" \
      "$(tr -d '\r' < "$c/build.obey" 2>/dev/null | awk 'length > 250 { n++ } END { print n + 0 }')" 0
}

# CTRL+F12: hub's shell, from hub 0.6. The change is saved first, a command
# typed at the shell runs there, and exit brings ade back on its file, with
# nothing to say.
case_25() {
local c
c=$(card "$proj/bin/ade.bin")
printf 'alpha\r\n' > "$c/shell.txt"
printf 'changed\r\n' > "$c/new.txt"
under_hub "$c" 120 "ade shell.txt
copy shell.txt typed.txt
delete shell.txt
rename new.txt shell.txt
exit"
check "CTRL+F12: the shell, with the window saved first" \
      "$(tr -d '\r\n' < "$c/typed.txt" 2>/dev/null)" "xalpha"
check "  and exit brings ade back on its file" \
      "$(grep '^footer ' "$c/ade.scr" 2>/dev/null)" "footer shell.txt"
# Only after exit: ade reads the file the shell put there, which it would not
# have if it had come back before the commands ran.
check "  only after the shell's commands ran" \
      "$(sed -n '3s/ *$//p' "$c/ade.scr" 2>/dev/null)" " changed"
check "  saying nothing" "$(grep '^said ' "$c/ade.scr" 2>/dev/null | tr -d '\r')" "said "
}

# CTRL+F12 on hub 0.5.0, which does not know HUB_SHELL and runs the job's
# command as a program instead -- with a shell.bin on the card that returns
# 0. The command is named so that nothing runs, and ade comes back saying
# the shell needs a newer hub; named "shell", the 0 would pass for an exit.
case_26() {
    [ -n "$OLDHUB" ] || return 0
local c
c=$(card "$proj/bin/ade.bin")
cp "$OLDHUB" "$c/mos/hub.bin"
cp "$zero/bin/zero.bin" "$c/bin/shell.bin"
printf 'alpha\r\n' > "$c/shell.txt"
under_hub "$c" 120 "ade shell.txt"
check "CTRL+F12 on hub 0.5: back, saying hub must be newer" \
      "$(grep '^said ' "$c/ade.scr" 2>/dev/null | tr -d '\r')" "said The shell needs hub 0.6 or later"
}

# ade started with no file shows its banner, which says how to reach the
# menus, until the first key.
case_27() {
local c
c=$(card "$proj/bin/ade.bin")
: > "$c/banner"
under_hub "$c" 60 "ade"
check "no file: the banner says F10 opens the menus" \
      "$(grep -c '| *Press F10 to access the menus *|' "$c/ade.scr" 2>/dev/null)" 1
check "  under ade's name" \
      "$(grep -c '| *ADE: Another Development Environment *|' "$c/ade.scr" 2>/dev/null)" 1
}

# run_cases <case>...: each case in the background, ADE_JOBS (4) at a time,
# its output kept apart; then all of it, in order. Each case has a card of its
# own, so none waits on another. A case's checks set status in its own shell,
# so the failures are counted from what they printed.
ADE_JOBS=${ADE_JOBS:-4}
run_cases() {
    local f
    for f in "$@"; do
        while [ "$(jobs -rp | wc -l)" -ge "$ADE_JOBS" ]; do
            wait -n
        done
        ( "$f" ) > "$W/$f.out" 2>&1 &
    done
    wait
    for f in "$@"; do
        cat "$W/$f.out"
        grep -q '^FAIL' "$W/$f.out" && status=1
    done
}

run_cases case_01 case_01b case_01c case_01d case_01e case_01f case_01g case_02 case_03 case_04 case_05 case_06 case_07 case_08 case_08b case_09 case_09b case_10 case_11 case_12 case_13 case_14 case_15a case_15b case_15 case_16 case_17 case_18 case_19 case_20 case_21 case_22 case_23 case_24 case_25 case_26 case_27

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
