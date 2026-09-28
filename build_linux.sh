#!/bin/bash
# Linux build (X11 + GLX). Output: build/linux/vr32x-track-editor and vrtool
set -e
cd "$(dirname "$0")"
OUT=build/linux; mkdir -p $OUT
CXX=${CXX:-g++}
FLAGS="-std=c++17 -O2 -Wall -Ithird_party -Ithird_party/imgui"
IMGUI="third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_impl_opengl3.cpp"
LIBS="-ldl"
for l in X11 GL; do if [ -e /usr/lib/x86_64-linux-gnu/lib$l.so ]; then LIBS="$LIBS -l$l"; else LIBS="$LIBS $(ls /usr/lib/x86_64-linux-gnu/lib$l.so.* | head -1)"; fi; done
$CXX $FLAGS -o $OUT/vr32x-track-editor src/platform_x11.cpp src/app.cpp src/renderer.cpp src/gl.cpp src/rom.cpp src/objio.cpp $IMGUI $LIBS
$CXX $FLAGS -o $OUT/vrtool tools/vrtool.cpp src/rom.cpp src/objio.cpp
echo "ok: $OUT"
