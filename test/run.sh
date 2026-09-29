#!/bin/bash
# ade's checks.
#
#   1. tools/fetch-aed-libs.sh takes the pinned package and nothing else.
#   2. ade builds against aed's two libraries, and links from them.
#   3. On the emulator, on the newest and the oldest firmware aed supports,
#      ade starts on a file named on the command line and opens it.
#
# The build and the emulator need a package: AED_LIBS=<unpacked package>, or
# the one aed-libs.pin names, fetched. The emulator run needs
# ~/fab-agon-emulator-1.2.4 (AGON_EMU overrides) and is skipped without it;
# the oldest firmware's MOS 2.3.3 is fetched from its release once and kept.
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

# A document too big for the memory ade gives one, so opening it pages -- and
# paging leaves its two scratch files beside it on the card while ade runs.
# The emulator is stopped with ade still in its key loop, so they are there
# to find: ade started, took the file from the command line, and opened it.
for fw in newest oldest; do
    card="$W/card-$fw"
    mkdir -p "$card/bin"
    cp -r "$EMU/sdcard/mos" "$card/"
    cp "$EMU/sdcard/MOS.bin" "$EMU/sdcard/firmware.bin" "$card/"
    cp bin/ade.bin "$card/bin/"
    for i in $(seq 1 3000); do
        printf 'line %04d of a document too long for memory\r\n' "$i"
    done > "$card/big.txt"
    printf 'ade big.txt\r\n' > "$card/autoexec.txt"
    chmod -R u+w "$card"

    flags=()
    [ "$fw" = oldest ] && flags=(--firmware quark --mos "$PWD/$MOS233/MOS.bin")
    fifo="$W/fifo-$fw"
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" &
    hold=$!
    (cd "$EMU" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout 30 ./fab-agon-emulator "${flags[@]}" --sdcard "$card" \
        < "$fifo" > /dev/null 2>&1)
    kill "$hold" 2>/dev/null
    wait "$hold" 2>/dev/null

    check "ade opens a file named on its command line ($fw firmware)" \
          "$([ -f "$card/big.txt.aedh" ] && [ -f "$card/big.txt.aedt" ] && echo opened)" opened
done

# --- 4. ade's windows, played on the emulator ---
# The test build of ade, with test/script in place of the keyboard -- see
# test/script/script.c for the keys and what they do -- as a project of its
# own, so the real build is left as it is.
case "$LIBS_DIR" in /*) LIBS_ABS=$LIBS_DIR ;; *) LIBS_ABS=$PWD/$LIBS_DIR ;; esac
proj="$W/scripted"
mkdir -p "$proj/src"
cp src/*.c src/*.h test/script/*.c test/script/*.h "$proj/src/"
cat > "$proj/Makefile" <<MK
NAME=ade
include \$(shell agondev-config --makefile)
CFLAGS += -DADE_SCRIPT -I$LIBS_ABS/include/core -I$LIBS_ABS/include/ui
PROJECTLIBDIR := $LIBS_ABS/lib
LIBS := -ledui -ledcore
MK
(cd "$proj" && make >/dev/null 2>&1)
check "the scripted ade builds" "$([ -f "$proj/bin/ade.bin" ] && echo built)" built

WANT="keys 42 of 42
current 2
window 1 a.txt xzalpha
window 2 b.txt y2#bravo
window 3 closed"

for fw in newest oldest; do
    card="$W/win-$fw"
    mkdir -p "$card/bin"
    cp -r "$EMU/sdcard/mos" "$card/"
    cp "$EMU/sdcard/MOS.bin" "$EMU/sdcard/firmware.bin" "$card/"
    cp "$proj/bin/ade.bin" "$card/bin/"
    printf 'alpha\r\n' > "$card/a.txt"
    printf 'bravo\r\n' > "$card/b.txt"
    printf 'charlie\r\n' > "$card/c.txt"
    printf 'ade a.txt\r\n' > "$card/autoexec.txt"
    chmod -R u+w "$card"

    flags=()
    [ "$fw" = oldest ] && flags=(--firmware quark --mos "$PWD/$MOS233/MOS.bin")
    fifo="$W/wfifo-$fw"
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" &
    hold=$!
    (cd "$EMU" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout 40 ./fab-agon-emulator "${flags[@]}" --sdcard "$card" \
        < "$fifo" > /dev/null 2>&1)
    kill "$hold" 2>/dev/null
    wait "$hold" 2>/dev/null

    got=$(tr -d '\r' < "$card/ade.out" 2>/dev/null)
    if [ "$got" = "$WANT" ]; then
        printf 'PASS  %-56s %s\n' "three windows, opened, moved between, closed ($fw)" "as played"
    else
        printf 'FAIL  %-56s\n' "three windows, opened, moved between, closed ($fw)"
        echo "      want: $(echo "$WANT" | tr '\n' '|')"
        echo "      got:  $(echo "${got:-nothing}" | tr '\n' '|')"
        status=1
    fi
    check "  and nothing was saved, as answered ($fw)" \
          "$(cat "$card/a.txt" "$card/b.txt" "$card/c.txt" | tr -d '\r' | tr '\n' ' ')" \
          "alpha bravo charlie "
done

exit $status
