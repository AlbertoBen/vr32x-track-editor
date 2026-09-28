// Shared command-line handling and frame loop helpers for both platforms.
#pragma once
#include "app.h"
#include "imgui.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

struct Options {
    std::string rom, screenshot, windowShot;
    int track = 1, frames = 5, selRow = -1, selCol = -1;
    float yaw = 0, pitch = 1.0f, zoom = 1.0f;
    bool view = false;
};

inline Options parseArgs(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--track") o.track = std::atoi(next().c_str());
        else if (a == "--screenshot") o.screenshot = next();
        else if (a == "--window-shot") o.windowShot = next();
        else if (a == "--frames") o.frames = std::atoi(next().c_str());
        else if (a == "--view") { std::sscanf(next().c_str(), "%f,%f,%f", &o.yaw, &o.pitch, &o.zoom); o.view = true; }
        else if (a.size() && a[0] != '-') o.rom = a;
    }
    return o;
}

// Load the first available system font with Latin-1 glyphs (accents, ñ, ¿, ¡).
inline void loadFont(float size, std::initializer_list<const char *> candidates) {
    ImGuiIO &io = ImGui::GetIO();
    for (const char *f : candidates) {
        std::ifstream test(f, std::ios::binary);
        if (!test) continue;
        if (io.Fonts->AddFontFromFileTTF(f, size, nullptr, io.Fonts->GetGlyphRangesDefault())) return;
    }
    io.Fonts->AddFontDefault();
}
