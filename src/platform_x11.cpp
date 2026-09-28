// Linux / X11 + GLX front end (minimal; mainly for development and automated tests).
#include "app.h"
#include "gl.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "main_common.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include "stb_image_write.h"

extern "C" {
typedef struct __GLXcontextRec *GLXContext;
XVisualInfo *glXChooseVisual(Display *, int, int *);
GLXContext glXCreateContext(Display *, XVisualInfo *, GLXContext, Bool);
Bool glXMakeCurrent(Display *, Window, GLXContext);
void glXSwapBuffers(Display *, Window);
void *glXGetProcAddressARB(const unsigned char *);
}

static void *getProc(const char *n) { return glXGetProcAddressARB(reinterpret_cast<const unsigned char *>(n)); }

struct X11Platform : Platform {
    Display *dpy = nullptr;
    Window win = 0;
    bool openFile(const char *, const char *, std::string &path) override { return askPath("Abrir: ruta del archivo", path); }
    bool saveFile(const char *, const char *, const char *, std::string &path) override { return askPath("Guardar: ruta del archivo", path); }
    void setTitle(const std::string &t) override { if (dpy) XStoreName(dpy, win, t.c_str()); }
    // No native dialog on X11: read a path from the terminal.
    bool askPath(const char *prompt, std::string &path) {
        std::printf("%s [%s]: ", prompt, path.c_str());
        std::fflush(stdout);
        char buf[1024];
        if (!std::fgets(buf, sizeof buf, stdin)) return false;
        std::string s(buf);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty()) path = s;
        return !path.empty();
    }
};

static ImGuiKey mapKey(KeySym k) {
    if (k >= XK_a && k <= XK_z) return ImGuiKey(ImGuiKey_A + (k - XK_a));
    if (k >= XK_A && k <= XK_Z) return ImGuiKey(ImGuiKey_A + (k - XK_A));
    if (k >= XK_0 && k <= XK_9) return ImGuiKey(ImGuiKey_0 + (k - XK_0));
    switch (k) {
    case XK_Tab: return ImGuiKey_Tab; case XK_Left: return ImGuiKey_LeftArrow; case XK_Right: return ImGuiKey_RightArrow;
    case XK_Up: return ImGuiKey_UpArrow; case XK_Down: return ImGuiKey_DownArrow; case XK_Home: return ImGuiKey_Home;
    case XK_End: return ImGuiKey_End; case XK_Delete: return ImGuiKey_Delete; case XK_BackSpace: return ImGuiKey_Backspace;
    case XK_Return: return ImGuiKey_Enter; case XK_Escape: return ImGuiKey_Escape; case XK_space: return ImGuiKey_Space;
    case XK_Shift_L: return ImGuiKey_LeftShift; case XK_Shift_R: return ImGuiKey_RightShift;
    case XK_Control_L: return ImGuiKey_LeftCtrl; case XK_Control_R: return ImGuiKey_RightCtrl;
    default: return ImGuiKey_None;
    }
}

int main(int argc, char **argv) {
    Options opt = parseArgs(argc, argv);
    X11Platform plat;
    plat.dpy = XOpenDisplay(nullptr);
    if (!plat.dpy) { std::fprintf(stderr, "no hay display X11\n"); return 1; }
    int attrs[] = {4 /*GLX_RGBA*/, 5 /*GLX_DOUBLEBUFFER*/, 12 /*GLX_DEPTH_SIZE*/, 24, 0};
    XVisualInfo *vi = glXChooseVisual(plat.dpy, DefaultScreen(plat.dpy), attrs);
    if (!vi) { std::fprintf(stderr, "sin visual GLX\n"); return 1; }
    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(plat.dpy, RootWindow(plat.dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | StructureNotifyMask;
    int W = 1280, H = 800;
    plat.win = XCreateWindow(plat.dpy, RootWindow(plat.dpy, vi->screen), 0, 0, W, H, 0, vi->depth, InputOutput,
                             vi->visual, CWColormap | CWEventMask, &swa);
    Atom wmDelete = XInternAtom(plat.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(plat.dpy, plat.win, &wmDelete, 1);
    XMapWindow(plat.dpy, plat.win);
    GLXContext ctx = glXCreateContext(plat.dpy, vi, nullptr, True);
    glXMakeCurrent(plat.dpy, plat.win, ctx);
    const char *missing = nullptr;
    if (!vrglLoad(getProc, &missing)) { std::fprintf(stderr, "falta %s\n", missing); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    loadFont(16.0f, {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf"});
    ImGui_ImplOpenGL3_Init("#version 330");
    App app;
    std::string err;
    if (!app.init(&plat, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    if (!opt.rom.empty()) { app.openRom(opt.rom); app.selectTrack(opt.track - 1); }
    if (opt.view) app.setViewportForTest(opt.yaw, opt.pitch, opt.zoom);
    bool automated = !opt.screenshot.empty() || !opt.windowShot.empty();

    auto last = std::chrono::steady_clock::now();
    for (int frame = 0; !app.quitRequested(); ++frame) {
        while (XPending(plat.dpy)) {
            XEvent e; XNextEvent(plat.dpy, &e);
            switch (e.type) {
            case ConfigureNotify: W = e.xconfigure.width; H = e.xconfigure.height; break;
            case MotionNotify: io.AddMousePosEvent(float(e.xmotion.x), float(e.xmotion.y)); break;
            case ButtonPress: case ButtonRelease: {
                bool down = e.type == ButtonPress;
                int b = e.xbutton.button;
                if (b == 1) io.AddMouseButtonEvent(0, down);
                else if (b == 3) io.AddMouseButtonEvent(1, down);
                else if (b == 2) io.AddMouseButtonEvent(2, down);
                else if (down && (b == 4 || b == 5)) io.AddMouseWheelEvent(0, b == 4 ? 1.0f : -1.0f);
                break;
            }
            case KeyPress: case KeyRelease: {
                bool down = e.type == KeyPress;
                char txt[16] = {0};
                KeySym ks;
                int n = XLookupString(&e.xkey, txt, sizeof txt - 1, &ks, nullptr);
                io.AddKeyEvent(ImGuiMod_Ctrl, (e.xkey.state & ControlMask) != 0);
                io.AddKeyEvent(ImGuiMod_Shift, (e.xkey.state & ShiftMask) != 0);
                ImGuiKey k = mapKey(ks);
                if (k != ImGuiKey_None) io.AddKeyEvent(k, down);
                if (down && n > 0 && (unsigned char)txt[0] >= 32) io.AddInputCharactersUTF8(txt);
                break;
            }
            case ClientMessage: if (Atom(e.xclient.data.l[0]) == wmDelete) return 0; break;
            }
        }
        auto now = std::chrono::steady_clock::now();
        io.DeltaTime = std::max(1e-4f, std::chrono::duration<float>(now - last).count());
        last = now;
        io.DisplaySize = ImVec2(float(W), float(H));
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        app.frame();
        ImGui::Render();
        glViewport(0, 0, W, H);
        glClearColor(0.1f, 0.1f, 0.12f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (automated && frame + 1 >= opt.frames) {
            if (!opt.screenshot.empty()) app.screenshotViewport(opt.screenshot);
            if (!opt.windowShot.empty()) {
                std::vector<unsigned char> px(size_t(W) * H * 4), flip(px.size());
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                for (int y = 0; y < H; ++y) std::memcpy(&flip[size_t(y) * W * 4], &px[size_t(H - 1 - y) * W * 4], size_t(W) * 4);
                stbi_write_png(opt.windowShot.c_str(), W, H, 4, flip.data(), W * 4);
            }
            break;
        }
        glXSwapBuffers(plat.dpy, plat.win);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
