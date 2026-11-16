#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 unemu contributors.
#
# Build the release assets of one tag of this repository:
#   bios-unemu-at.bin                  the unemu-at flake output (32 KiB ROM)
#   unemu-at-bios-<version>-src.tar    its corresponding source
#   SHA256SUMS                         the SHA-256 of both
#
# The source archive is deterministic: the tree Nix builds the ROM from (the
# tracked files at the tag's revision) without upstream's prebuilt images
# (binaries/, xtide_r625/), plus a NOTICE made from source-notice.txt.in, in
# a GNU tar ustar archive with fixed member order, timestamps (the commit
# time), owners and modes. The archived tree must build the same ROM. The
# recipe matches the one unemupc's firmware fetch script pins, so both give
# the same bytes.
#
# Usage: release/assets.sh <tag> <output directory>
# Run it from a clone that has the tag, inside `nix develop` (GNU tar, git).
set -euo pipefail

if [ $# -ne 2 ]; then
	echo "usage: $0 <tag> <output directory>" >&2
	exit 2
fi
TAG="$1"
OUT="$2"

unset TAR_OPTIONS
case "$(tar --version 2>/dev/null)" in
	*"GNU tar"*) ;;
	*)
		echo "GNU tar is required: run this script inside nix develop" >&2
		exit 1
		;;
esac

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
REVISION="$(git -C "$REPO" rev-parse --verify "refs/tags/$TAG^{commit}")"
VERSION="${TAG#v}"
SOURCE_NAME="unemu-at-bios-$VERSION-src"
FLAKE="git+file://$REPO?ref=refs/tags/$TAG&rev=$REVISION"
NIX=(nix --extra-experimental-features "nix-command flakes")

echo "building $TAG ($REVISION)"
ROM_STORE="$("${NIX[@]}" build --no-link --print-out-paths "$FLAKE#unemu-at")"

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
WORK="$(mktemp -d)"
trap 'chmod -R u+w "$WORK" 2>/dev/null || true; rm -rf "$WORK"' EXIT

# The raw 32 KiB image; the CPU reset entry F000:FFF0 is offset 7FF0h, a far
# jump (EAh) into POST. The flake has already checked the pinned SHA-256.
cp "$ROM_STORE/bios-unemu-at.bin" "$WORK/bios-unemu-at.bin"
chmod 0644 "$WORK/bios-unemu-at.bin"
SIZE="$(wc -c < "$WORK/bios-unemu-at.bin" | tr -d '[:space:]')"
RESET="$(od -An -tx1 -j 32752 -N1 "$WORK/bios-unemu-at.bin" | tr -d ' \n')"
if [ "$SIZE" -ne 32768 ] || [ "$RESET" != "ea" ]; then
	echo "invalid system BIOS: expected 32768 bytes and a far jump at offset 7FF0h" >&2
	exit 1
fi
ROM_SHA256="$(sha256sum "$WORK/bios-unemu-at.bin" | awk '{print $1}')"

# The tree Nix built the ROM from; its commit time becomes every member's
# timestamp.
SOURCE_INFO="$(UNEMU_AT_BIOS_FLAKE_REF="$FLAKE" "${NIX[@]}" eval --raw --impure --expr \
	'let flake = builtins.getFlake (builtins.getEnv "UNEMU_AT_BIOS_FLAKE_REF"); in "${toString flake.lastModified} ${flake.outPath}"')"
SOURCE_EPOCH="${SOURCE_INFO%% *}"
SOURCE_TREE="${SOURCE_INFO#* }"
STAGE="$WORK/source/$SOURCE_NAME"
mkdir -p "$STAGE"
cp -R "$SOURCE_TREE" "$STAGE/unemu-at-bios"
chmod -R u+w "$STAGE"
# Upstream's prebuilt images are build outputs, not source, and the ROM does
# not contain them: binaries/ holds other boards' BIOS images, xtide_r625/
# XTIDE Universal BIOS r625 binaries whose source the archive does not carry.
rm -rf "$STAGE/unemu-at-bios/binaries" "$STAGE/unemu-at-bios/xtide_r625"
sed -e "s|@SOURCE_NAME@|$SOURCE_NAME|g" -e "s|@TAG@|$TAG|g" \
	-e "s|@REVISION@|$REVISION|g" -e "s|@ROM_SHA256@|$ROM_SHA256|g" \
	"$HERE/source-notice.txt.in" > "$STAGE/NOTICE"

# The archived tree, a flake of its own, must still build the same ROM.
REBUILT="$("${NIX[@]}" build --no-link --print-out-paths --no-write-lock-file \
	"path:$STAGE/unemu-at-bios#unemu-at")"
if [ "$(sha256sum "$REBUILT/bios-unemu-at.bin" | awk '{print $1}')" != "$ROM_SHA256" ]; then
	echo "the archived tree does not build the same ROM" >&2
	exit 1
fi

LC_ALL=C tar --create --file="$WORK/source.tar" --directory="$WORK/source" \
	--format=ustar --sort=name --hard-dereference --mtime="@$SOURCE_EPOCH" \
	--owner=0 --group=0 --numeric-owner --mode='a=rX,u+w,ug-s' "$SOURCE_NAME"

mv "$WORK/bios-unemu-at.bin" "$OUT/bios-unemu-at.bin"
mv "$WORK/source.tar" "$OUT/$SOURCE_NAME.tar"
(cd "$OUT" && sha256sum bios-unemu-at.bin "$SOURCE_NAME.tar" > SHA256SUMS)
cat "$OUT/SHA256SUMS"
