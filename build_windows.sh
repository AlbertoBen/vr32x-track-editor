#!/bin/bash
# Cross-compile the Windows version with llvm-mingw (https://github.com/mstorsjo/llvm-mingw).
# Usage: MINGW=/path/to/llvm-mingw ./build_windows.sh   -> build/windows/*.exe
set -e
cd "$(dirname "$0")"
MINGW=${MINGW:-/home/claude/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64}
CXX=$MINGW/bin/x86_64-w64-mingw32-g++
OUT=build/windows; mkdir -p $OUT
FLAGS="-std=c++17 -O2 -Wall -Ithird_party -Ithird_party/imgui -static -DUNICODE -D_UNICODE"
IMGUI="third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_impl_opengl3.cpp third_party/imgui/imgui_impl_win32.cpp"
$MINGW/bin/x86_64-w64-mingw32-windres res/app.rc -O coff -o $OUT/app.res
$CXX $FLAGS -mwindows -municode -o $OUT/vr32x-track-editor.exe src/platform_win32.cpp src/app.cpp src/renderer.cpp src/gl.cpp src/rom.cpp src/objio.cpp $IMGUI $OUT/app.res \
    -lopengl32 -lgdi32 -luser32 -lcomdlg32 -lshell32 -ldwmapi
$CXX $FLAGS -o $OUT/vrtool.exe tools/vrtool.cpp src/rom.cpp src/objio.cpp
rm -f $OUT/app.res
echo "ok: $OUT"
