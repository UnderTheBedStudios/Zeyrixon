#!/usr/bin/env bash
#
# Removes the .zeyrixon file association installed by install-file-association.sh.

set -euo pipefail

MIME_TYPE="application/x-zeyrixon-project"
DESKTOP_ID="zeyrixoneditor.desktop"

MIME_DEST="$HOME/.local/share/mime/packages/zeyrixon-project.xml"
DESKTOP_DEST="$HOME/.local/share/applications/$DESKTOP_ID"

rm -f "$MIME_DEST" "$DESKTOP_DEST"

update-mime-database "$HOME/.local/share/mime" >/dev/null 2>&1 || true
command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true

echo "Removed .zeyrixon file association ($MIME_TYPE / $DESKTOP_ID)."