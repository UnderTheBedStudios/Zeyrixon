#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PROJECT_DIR="$(pwd)"

PREMAKE_BIN="$PROJECT_DIR/vendor/binary/premake5"

if [ -f "$PREMAKE_BIN" ] && [ ! -x "$PREMAKE_BIN" ]; then
    echo "Applying executable permissions to local Premake binary..."
    chmod +x "$PREMAKE_BIN"
fi

echo "Generating project build files..."
"$PREMAKE_BIN" gmake2
"$PREMAKE_BIN" ninja