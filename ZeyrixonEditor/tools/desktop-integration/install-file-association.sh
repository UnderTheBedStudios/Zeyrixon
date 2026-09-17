#!/usr/bin/env bash
#
# Registers *.zeyrixon as a file type that opens in ZeyrixonEditor, so double-
# clicking one in a file manager (Nautilus, Dolphin, Thunar, ...) launches the
# editor with that project loaded.
#
# This is a per-user install (writes under ~/.local/share) - no root/sudo
# needed, and it won't clash with other users on the same machine.
#
# Usage:
#   ZeyrixonEditor/tools/desktop-integration/install-file-association.sh [config]
#
# [config] is the premake configuration to point at: Debug (default),
# Release, or Dist. Re-run this script if you rebuild under a different
# config and want file-opens to use that binary instead.

set -euo pipefail

CONFIG="${1:-Debug}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

BINARY_PATH="$PROJECT_ROOT/bin/${CONFIG}-linux-x86_64/ZeyrixonEditor/ZeyrixonEditor"
ICON_PATH="$PROJECT_ROOT/Icon.png"
MIME_SRC="$SCRIPT_DIR/zeyrixon-project.xml"
DESKTOP_TEMPLATE="$SCRIPT_DIR/zeyrixoneditor.desktop"

MIME_TYPE="application/x-zeyrixon-project"
DESKTOP_ID="zeyrixoneditor.desktop"

MIME_DEST_DIR="$HOME/.local/share/mime/packages"
APPS_DEST_DIR="$HOME/.local/share/applications"

if [ ! -x "$BINARY_PATH" ]; then
    echo "error: no built editor binary at:" >&2
    echo "  $BINARY_PATH" >&2
    echo "Build the '$CONFIG' configuration first (e.g. 'make config=${CONFIG,,} ZeyrixonEditor')," >&2
    echo "or pass a different config to this script, e.g.:" >&2
    echo "  $0 Release" >&2
    exit 1
fi

mkdir -p "$MIME_DEST_DIR" "$APPS_DEST_DIR"

# 1. Register the MIME type (glob *.zeyrixon -> application/x-zeyrixon-project)
cp "$MIME_SRC" "$MIME_DEST_DIR/zeyrixon-project.xml"
update-mime-database "$HOME/.local/share/mime" >/dev/null

# 2. Generate the .desktop launcher pointing at this build's binary
sed \
    -e "s|@EXEC_PATH@|$BINARY_PATH|g" \
    -e "s|@ICON_PATH@|$ICON_PATH|g" \
    "$DESKTOP_TEMPLATE" > "$APPS_DEST_DIR/$DESKTOP_ID"
chmod +x "$APPS_DEST_DIR/$DESKTOP_ID"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPS_DEST_DIR" >/dev/null
fi

# 3. Make it the default handler for the MIME type
if command -v xdg-mime >/dev/null 2>&1; then
    xdg-mime default "$DESKTOP_ID" "$MIME_TYPE"
else
    echo "warning: xdg-mime not found; couldn't set ZeyrixonEditor as the default handler." >&2
    echo "Install xdg-utils, or set it manually via your file manager's 'Open With' dialog." >&2
fi

echo "Done. .zeyrixon files now open in ZeyrixonEditor ($CONFIG build)."
echo "  Binary:  $BINARY_PATH"
echo "  Desktop: $APPS_DEST_DIR/$DESKTOP_ID"
echo ""
echo "If a file manager was already open, you may need to restart it (or log out/in)"
echo "for the new association to show up in 'Open With'."