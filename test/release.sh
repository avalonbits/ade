#!/bin/bash
# Checks a release zip. ade-<version>.zip must hold ade, aed's configuration
# and ade/, and nothing of the tools. ade-full-<version>.zip is checked on the
# emulator: unzipped onto a fresh card -- MOS and nothing else -- what ade
# drives must work from it alone. acc compiles and
# links a C program that runs; zap assembles, and writes acc objects; ref
# finds a word in both documents, and nothing for a word in neither; md2ref
# makes a document.
#
# Usage: test/release.sh dist/ade-<version>.zip | dist/ade-full-<version>.zip
set -uo pipefail
cd "$(dirname "$0")/.."

ZIP=${1:?usage: test/release.sh dist/ade-<version>.zip | dist/ade-full-<version>.zip}
EMU=${AGON_EMU:-$HOME/fab-agon-emulator-1.2.4}
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
c="$W/card"
mkdir -p "$c"
cp -r "$EMU/sdcard/mos" "$c/"
cp "$EMU/sdcard/MOS.bin" "$EMU/sdcard/firmware.bin" "$c/"
unzip -oq "$ZIP" -d "$c"

# The small zip: ade, aed's configuration, ade/ -- and nothing else.
case $(basename "$ZIP") in
    ade-full-*) ;;
    *)
        for f in bin/ade.bin config/aed/syntax/c.cfg config/aed/themes/dark.cfg \
                 config/aed/unscii8.bin ade/README.md ade/COPYING ade/VERSIONS.txt; do
            check "in the zip: $f" "$([ -s "$c/$f" ] && echo yes)" yes
        done
        check "  and nothing but ade in bin/, and no mos/, lib/ or config/ref/" \
              "$(unzip -Z1 "$ZIP" | grep -E '^(bin/.|mos/|lib/|config/ref/)' | tr '\n' ' ')" \
              "bin/ade.bin "
        exit $status
        ;;
esac

for f in bin/ade.bin bin/acc.bin bin/zap.bin bin/ref.bin bin/md2ref.bin bin/aed.bin \
         mos/hub.bin lib/acc/libc.a lib/acc/rt.a lib/acc/libhub.a lib/acc/include/stdio.h \
         lib/acc/include/hub/hub.h config/ref/docs/mos.dat config/ref/docs/acc.dat \
         config/aed/syntax/c.cfg ade/README.md ade/COPYING ade/VERSIONS.txt; do
    check "in the zip: $f" "$([ -s "$c/$f" ] && echo yes)" yes
done

mkdir -p "$c/t"
printf '#include <stdio.h>\r\nint main(void)\r\n{\r\n    FILE* f = fopen("/t/ran.txt", "w");\r\n    fputs("ran\\n", f);\r\n    fclose(f);\r\n    return 0;\r\n}\r\n' > "$c/t/hello.c"
printf '        .assume adl=1\r\n        .org $40000\r\n        ret\r\n' > "$c/t/one.s"
printf '        XDEF    _seven\r\n        SEGMENT CODE\r\n_seven:\r\n        ld      hl, 7\r\n        ret\r\n' > "$c/t/seven.s"
printf '# Notes\r\n\r\n## `widget`\r\n\r\nA widget.\r\n' > "$c/t/notes.md"
# ref prints nothing the emulator shows outside, so it is asked by its exit
# code: 0 when it found something. Obey stops at a command that fails, so the
# copy after each look-up makes its marker only when ref found the word --
# and xyzzy, found nowhere, must leave none.
for look in "mos mos_fopen" "acc fopen" "mos xyzzy"; do
    set -- $look
    printf 'ref -p %s -k %s\r\ncopy notes.md %s-%s.ok\r\n' "$1" "$2" "$1" "$2" > "$c/t/$1-$2.obey"
done
cat > "$c/script.txt" <<'S'
cd t
acc hello.c -o hello.bin
hello
zap one.s one.bin
zap seven.s seven.o -f acc -c
md2ref -o notes.dat notes.md
Obey mos-mos_fopen.obey
Obey acc-fopen.obey
Obey mos-xyzzy.obey
emulator_exit_success
S
printf 'hub -f /script.txt\r\n' > "$c/autoexec.txt"
fifo="$W/fifo"
mkfifo "$fifo"
tail -f /dev/null > "$fifo" &
hold=$!
(cd "$EMU" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy \
    timeout 300 ./fab-agon-emulator -u --sdcard "$c" < "$fifo" > "$W/console" 2>&1)
kill "$hold" 2>/dev/null

check "acc compiles and links a C program, which runs" "$(tr -d '\r\n' < "$c/t/ran.txt" 2>/dev/null)" ran
check "zap assembles" "$([ -s "$c/t/one.bin" ] && echo yes)" yes
check "  and writes an object for acc, as a project's build asks" \
      "$([ -s "$c/t/seven.o" ] && echo yes)" yes
check "md2ref makes a document" "$([ -s "$c/t/notes.dat" ] && echo yes)" yes
check "ref finds mos_fopen in mos" "$([ -e "$c/t/mos-mos_fopen.ok" ] && echo yes)" yes
check "ref finds fopen in acc" "$([ -e "$c/t/acc-fopen.ok" ] && echo yes)" yes
check "  and nothing for xyzzy, so the two above are real" \
      "$([ -e "$c/t/mos-xyzzy.ok" ] && echo found || echo nothing)" nothing

exit $status
