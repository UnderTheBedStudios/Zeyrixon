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
"$PREMAKE_BIN" export-compile-commands

CONFIG="debug"
MERGED_CC="$PROJECT_DIR/compile_commands/${CONFIG}.json"

echo "Linking compile_commands.json into project folders..."
for proj in Zeyrixon ZeyrixonEditor TestProj; do
    ln -sf "$MERGED_CC" "$PROJECT_DIR/$proj/compile_commands.json"
done

CXX_PATH="$(command -v g++ || echo /usr/bin/g++)"

echo "Writing .vscode/c_cpp_properties.json for each project..."
for proj in Zeyrixon ZeyrixonEditor TestProj; do
    mkdir -p "$PROJECT_DIR/$proj/.vscode"
    cat > "$PROJECT_DIR/$proj/.vscode/c_cpp_properties.json" <<EOF
{
    "configurations": [
        {
            "name": "Linux",
            "compilerPath": "$CXX_PATH",
            "cStandard": "c17",
            "cppStandard": "c++17",
            "intelliSenseMode": "linux-gcc-x64",
            "compileCommands": "\${workspaceFolder}/compile_commands.json"
        }
    ],
    "version": 4
}
EOF
done