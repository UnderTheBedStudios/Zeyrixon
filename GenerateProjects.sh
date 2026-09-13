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

echo "Writing root .vscode/c_cpp_properties.json..."
mkdir -p "$PROJECT_DIR/.vscode"
cat > "$PROJECT_DIR/.vscode/c_cpp_properties.json" <<'EOF'
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/Zeyrixon/src",
                "${workspaceFolder}/Zeyrixon/vendor/spdlog/include",
                "${workspaceFolder}/Zeyrixon/vendor/GLFW/include",
                "${workspaceFolder}/Zeyrixon/vendor/GLAD/include",
                "${workspaceFolder}/Zeyrixon/vendor/stb",
                "${workspaceFolder}/ZeyrixonEditor/src",
                "${workspaceFolder}/ZeyrixonEditor/vendor/imgui",
                "${workspaceFolder}/ZeyrixonEditor/vendor/imgui/backends",
                "${workspaceFolder}/TestProj/src"
            ],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c17",
            "cppStandard": "c++17",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
EOF