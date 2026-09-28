// Editor application (platform independent, Dear ImGui).
#pragma once
#include "renderer.h"
#include "rom.h"
#include <string>

struct Platform {
    // File dialogs. Return false if cancelled. filter: "Nombre|*.ext" pairs separated by '|'.
    virtual bool openFile(const char *title, const char *filter, std::string &path) = 0;
    virtual bool saveFile(const char *title, const char *filter, const char *defExt, std::string &path) = 0;
    virtual void setTitle(const std::string &title) = 0;
    virtual ~Platform() = default;
};

class App {
public:
    bool init(Platform *platform, std::string &error);
    void frame();                        // build UI and render the 3D view
    bool quitRequested() const { return quit_; }
    bool openRom(const std::string &path);
    void selectTrack(int index);
    bool screenshotViewport(const std::string &pngPath);   // for automated tests
    void setViewportForTest(float yaw, float pitch, float zoom);

private:
    Platform *platform_ = nullptr;
    vr::Rom rom_;
    bool romLoaded_ = false, romDirty_ = false, quit_ = false;
    int track_ = 0;
    std::vector<uint32_t> blockKeys_;    // renderer block id -> Track::blocks key
    int selected_ = -1, hover_ = -1;
    TrackRenderer renderer_;
    Camera camera_;
    RenderOptions options_;
    std::string status_;
    bool showPalette_ = false, showHelp_ = false, showAbout_ = false;
    int viewW_ = 0, viewH_ = 0;

    void rebuild(bool reframe);
    void mainMenu();
    void leftPanel();
    void viewport();
    void cellMap();
    void blockInfo();
    void paletteWindow();
    void helpWindow();
    void exportTrack(bool onlySelected);
    void importObjFile();
    void saveRom();
    void updateTitle();
    int blockIdAt(int row, int col) const;
    bool cellOfBlock(int id, int &row, int &col) const;
};
