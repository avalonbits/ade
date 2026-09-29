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

# under_hub <card> <seconds> <command>: boot MOS 3.0.2 with hub running
# <command> from a script, as a person would at hub's prompt, and stop the
# emulator after <seconds>. Whatever ade wrote is on the card. Run flat out
# (-u): acc is a quarter of a megabyte to load and run, and nothing here is
# about how long anything takes.
under_hub() {
    local c=$1 secs=$2 fifo hold
    printf '%s\n' "$3" > "$c/script.txt"
    printf 'hub -f /script.txt\r\n' > "$c/autoexec.txt"
    chmod -R u+w "$c"
    fifo=$(mktemp -u -p "$W")
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" &
    hold=$!
    (cd "$EMU" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout "$secs" ./fab-agon-emulator -u --sdcard "$c" < "$fifo" > /dev/null 2>&1)
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
LIBS := -ledui -ledcore $HUB/build/lib/libhub.a
MK
(cd "$proj" && make >/dev/null 2>&1)
check "the scripted ade builds" "$([ -f "$proj/bin/ade.bin" ] && echo built)" built

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
window 1 a.txt 1:1 alpha
window 2 b.txt 1:4 y2#bravo
window 3 closed"
check "  and nothing was saved, as answered" \
      "$(cat "$c/a.txt" "$c/b.txt" "$c/c.txt" | tr -d '\r' | tr '\n' ' ')" \
      "alpha bravo charlie "

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
window 1 bad.c 3:12 int main(void)
window 2 closed
window 3 closed"
check "  where acc said it was" "$(tr -d '\r' < "$c/ade.err" 2>/dev/null | head -1)" \
      "bad.c:3:12: error: 'x' is not declared"

c=$(card "$proj/bin/ade.bin")
printf '  nop\r\n  frob a\r\n' > "$c/bad.s"
under_hub "$c" 120 "ade bad.s"
played "$c" "F9 on a broken assembly file: back on zap's error" "keys 2 of 2
current 1
at 2:3
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
window 1 good.txt 1:7 bravoy
window 2 good.c 3:2 int main(void)
window 3 closed"
check "  every window was saved first" \
      "$(sed -n 3p "$c/good.c" | tr -d '\r'; tr -d '\r' < "$c/good.txt")" \
      "     return 0;
bravoy"
check "  the program is on the card" "$([ -s "$c/good.bin" ] && echo yes)" yes

exit $status
