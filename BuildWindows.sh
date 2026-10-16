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
