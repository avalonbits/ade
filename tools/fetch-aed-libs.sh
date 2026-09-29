#!/bin/bash
# Makes the pinned aed-libs package available, and prints the directory it is
# unpacked in: third_party/aed-libs-<version>, holding lib/libedcore.a,
# lib/libedui.a, include/core and include/ui.
#
# The package is aed's release asset, pinned in aed-libs.pin by version and
# sha256. It is downloaded once, checked, and unpacked; later runs find it in
# place and print the directory straight away. A download whose checksum does
# not match the pin is refused and removed, so a build never runs against
# libraries nobody chose.
#
# AED_LIBS_URL overrides where the package comes from (a file:// URL works),
# for testing; the checksum is still enforced.
set -euo pipefail
cd "$(dirname "$0")/.."

PIN=aed-libs.pin
VERSION=$(sed -n 's/^version[[:space:]]\+//p' "$PIN")
SHA=$(sed -n 's/^sha256[[:space:]]\+//p' "$PIN")
if [ -z "$VERSION" ] || [ -z "$SHA" ]; then
    echo "fetch-aed-libs: $PIN needs a version and a sha256" >&2

    exit 1
fi
if ! [[ "$SHA" =~ ^[0-9a-f]{64}$ ]]; then
    echo "fetch-aed-libs: $PIN has no sha256 for $VERSION yet ($SHA);" >&2
    echo "                build with AED_LIBS=<unpacked package> meanwhile" >&2

    exit 1
fi

NAME="aed-libs-$VERSION"
DIR="third_party/$NAME"
STAMP="$DIR/.sha256"
if [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$SHA" ]; then
    echo "$DIR"

    exit 0
fi

URL=${AED_LIBS_URL:-https://github.com/avalonbits/aed/releases/download/v$VERSION/$NAME.tar.gz}
mkdir -p third_party
TGZ="third_party/$NAME.tar.gz"
rm -rf "$DIR" "$TGZ"
if ! curl -fsSL -o "$TGZ" "$URL"; then
    rm -f "$TGZ"
    echo "fetch-aed-libs: could not download $URL" >&2

    exit 1
fi
GOT=$(sha256sum "$TGZ" | cut -d' ' -f1)
if [ "$GOT" != "$SHA" ]; then
    rm -f "$TGZ"
    echo "fetch-aed-libs: $NAME.tar.gz has sha256 $GOT, and $PIN says $SHA" >&2

    exit 1
fi
tar -C third_party -xzf "$TGZ"
rm -f "$TGZ"
echo "$SHA" > "$STAMP"
echo "$DIR"
