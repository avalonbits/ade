#!/bin/bash
# Builds ade's release: one zip to unzip at the root of an SD card, with
# everything ade needs and uses beside it.
#
#   dist/ade-<version>.zip
#       bin/ade.bin                     ade, built here
#       bin/acc.bin, lib/acc/...        acc, its C library and headers
#       bin/zap.bin                     zap
#       mos/hub.bin, lib/acc/...hub     hub, and its client library for acc
#       bin/aed.bin, config/aed/...     aed, and the grammars, themes and
#                                       fonts ade shares with it
#       bin/ref.bin, bin/md2ref.bin,
#       config/ref/docs/*.dat           ref, md2ref, and the documents
#       ade/README.md, ade/COPYING,
#       ade/VERSIONS.txt                what this is, and what is in it
#
# What goes in beside ade is pinned in release.pin. Nothing is published:
# the zip is made locally, to be checked and then attached to a release.
#
# Usage: tools/mkrelease.sh <version>
#   VERSION_<name>=<v>   overrides a component's pinned version, for a trial
#   REF_DIR=<dir>        ref's checkout, ~/code/ref by default
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=${1:?usage: tools/mkrelease.sh <version>}
REF_DIR=${REF_DIR:-$HOME/code/ref}
grep -q "^#define ADE_VERSION \"$VERSION\"" src/version.h || {
    echo "mkrelease: src/version.h does not say $VERSION" >&2
    exit 1
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
CARD="$WORK/card"
mkdir -p "$CARD/bin" "$CARD/ade" dist

# What VERSIONS.txt says, a line per component.
versions() {
    printf '%-6s %-10s %s\n' "$@" >> "$CARD/ade/VERSIONS.txt"
}

# github <repo> <version> <assets>: each asset of release v<version>, a zip
# unpacked at the card's root or a .bin put in /bin.
github() {
    local repo=$1 v=$2 asset
    for asset in ${3//,/ }; do
        asset=${asset//\{v\}/$v}
        curl -fsSL -o "$WORK/$asset" "https://github.com/$repo/releases/download/v$v/$asset" || {
            echo "mkrelease: no $asset in $repo v$v" >&2
            exit 1
        }
        case $asset in
            *.zip) unzip -oq "$WORK/$asset" -d "$CARD" ;;
            *.bin) cp "$WORK/$asset" "$CARD/bin/$asset" ;;
        esac
        versions "" "" "$asset $(sha256sum "$WORK/$asset" | cut -c1-16)"
    done
}

# ref <commit>: ref and its documents, made by ref's own mkrelease.sh from
# that commit. Its documents include mos.dat, from agon-docs, which has no
# license of its own: Igor chose to ship it (2026-10-12), which is what
# REF_LICENSE_SETTLED says to ref's script.
ref() {
    local commit=$1
    git -C "$REF_DIR" worktree add --detach "$WORK/ref" "$commit" >/dev/null 2>&1
    mkdir -p "$WORK/ref/bin" "$WORK/ref/obj" "$WORK/refout"
    (cd "$WORK/ref" && REF_LICENSE_SETTLED=1 ./mkrelease.sh "$WORK/refout" >/dev/null)
    git -C "$REF_DIR" worktree remove --force "$WORK/ref"
    unzip -oq "$WORK"/refout/ref-[0-9]*.zip -d "$CARD"
}

printf 'ade %s, and what it bundles:\n\n' "$VERSION" > "$CARD/ade/VERSIONS.txt"
while read -r name version how; do
    case $name in ''|'#'*) continue ;; esac
    override="VERSION_$name"
    version=${!override:-$version}
    if [ "$version" = pending ]; then
        echo "mkrelease: $name has no release ade can bundle yet (release.pin)" >&2
        exit 1
    fi
    versions "$name" "$version" "$how"
    case $how in
        github:*) IFS=: read -r _ repo assets <<< "$how"; github "$repo" "$version" "$assets" ;;
        ref:*) ref "${how#ref:}" ;;
    esac
done < release.pin

# ade, built fresh against its pinned libraries.
make -s clean >/dev/null
make -s >/dev/null
cp bin/ade.bin "$CARD/bin/ade.bin"
cp README.md COPYING "$CARD/ade/"
versions ade "$VERSION" "$(git rev-parse --short HEAD)"

rm -f "dist/ade-$VERSION.zip"
(cd "$CARD" && zip -qr "$OLDPWD/dist/ade-$VERSION.zip" .)
echo "dist/ade-$VERSION.zip"
