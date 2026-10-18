#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

CONFIG="${1:-dist}"          # debug | release | dist
TARGET="${2:-}"              # optional: Zeyrixon, ZeyrixonEditor, TestProj

CC=x86_64-w64-mingw32-gcc
CXX=x86_64-w64-mingw32-g++

# Fail early if the cross compiler isn't installed
for tool in "$CC" "$CXX"; do
    command -v "$tool" >/dev/null || {
        echo "Missing $tool (Arch: sudo pacman -S mingw-w64-gcc mingw-w64-winpthreads)"
        exit 1
    }
done

echo "Generating Windows Makefiles..."
./vendor/binary/premake5 gmake --os=windows --cc=gcc

echo "Building ($CONFIG)..."
make -j"$(nproc)" config="$CONFIG" CC="$CC" CXX="$CXX" $TARGET

echo "Done. Output:"
ls -lh bin/*-windows-x86_64/*/*.exe 2>/dev/null || true

# zip the editor so it can be sent as one file.
if command -v zip >/dev/null; then
    for dir in bin/*-windows-x86_64/ZeyrixonEditor/; do
        [ -d "$dir" ] || continue
        out="$(pwd)/bin/ZeyrixonEditor-$(basename "$(dirname "$dir")")-windows.zip"
        rm -f "$out"
        (cd "$dir" && zip -qr "$out" . -x '*.pdb' '*.ilk')
        echo "Packaged: $out"
    done
fi
