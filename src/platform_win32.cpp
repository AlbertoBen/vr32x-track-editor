// Windows front end: Win32 window + WGL OpenGL context + Dear ImGui Win32 backend.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include "app.h"
#include "gl.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "main_common.h"
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static HMODULE g_opengl32 = nullptr;
static void *getProc(const char *name) {
    void *p = reinterpret_cast<void *>(wglGetProcAddress(name));
    if (p == nullptr || p == reinterpret_cast<void *>(1) || p == reinterpret_cast<void *>(2) ||
        p == reinterpret_cast<void *>(3) || p == reinterpret_cast<void *>(-1))
        p = reinterpret_cast<void *>(GetProcAddress(g_opengl32, name));
    return p;
}

static std::wstring widen(const std::string &s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    w.resize(size_t(n - 1));
    return w;
}
static std::string narrow(const wchar_t *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    s.resize(size_t(n - 1));
    return s;
}

struct Win32Platform : Platform {
    HWND hwnd = nullptr;
    static std::wstring filterString(const char *filter) {      // "A|*.a|B|*.b" -> "A\0*.a\0B\0*.b\0\0"
        std::wstring f = widen(filter);
        for (auto &c : f) if (c == L'|') c = L'\0';
        f.push_back(L'\0'); f.push_back(L'\0');
        return f;
    }
    bool dialog(bool save, const char *title, const char *filter, const char *defExt, std::string &path) {
        wchar_t buf[MAX_PATH * 4] = {0};
        std::wstring initial = widen(path);
        std::wstring dir;
        size_t slash = initial.find_last_of(L"/\\");
        if (slash != std::wstring::npos) { dir = initial.substr(0, slash); initial = initial.substr(slash + 1); }
        wcsncpy(buf, initial.c_str(), MAX_PATH * 4 - 1);
        std::wstring f = filterString(filter), t = widen(title), ext = defExt ? widen(defExt) : L"";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof ofn;
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = f.c_str();
        ofn.lpstrFile = buf;
        ofn.nMaxFile = MAX_PATH * 4;
        ofn.lpstrTitle = t.c_str();
        ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
        ofn.lpstrDefExt = ext.empty() ? nullptr : ext.c_str();
        ofn.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
        if (!ok) return false;
        path = narrow(buf);
        return true;
    }
    bool openFile(const char *title, const char *filter, std::string &path) override { return dialog(false, title, filter, nullptr, path); }
    bool saveFile(const char *title, const char *filter, const char *defExt, std::string &path) override { return dialog(true, title, filter, defExt, path); }
    void setTitle(const std::string &t) override { SetWindowTextW(hwnd, widen(t).c_str()); }
};

static App *g_app = nullptr;
static int g_width = 1280, g_height = 800;

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) { g_width = LOWORD(lParam); g_height = HIWORD(lParam); }
        return 0;
    case WM_DROPFILES: {
        wchar_t file[MAX_PATH * 4];
        if (g_app && DragQueryFileW(reinterpret_cast<HDROP>(wParam), 0, file, MAX_PATH * 4)) g_app->openRom(narrow(file));
        DragFinish(reinterpret_cast<HDROP>(wParam));
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;       // no ALT application menu
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(narrow(wargv[i]));
    std::vector<char *> argv;
    for (auto &a : args) argv.push_back(&a[0]);
    Options opt = parseArgs(argc, argv.data());

    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = {sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, hInst, LoadIconW(hInst, L"IDI_APPICON"),
                      LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"VR32XTrackEditor", nullptr};
    RegisterClassExW(&wc);
    float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));
    Win32Platform plat;
    plat.hwnd = CreateWindowW(wc.lpszClassName, L"VR32X Track Editor", WS_OVERLAPPEDWINDOW, 100, 100,
                              int(1280 * scale), int(800 * scale), nullptr, nullptr, wc.hInstance, nullptr);
    DragAcceptFiles(plat.hwnd, TRUE);

    HDC hdc = GetDC(plat.hwnd);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof pfd; pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.cStencilBits = 8;
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf == 0 || !SetPixelFormat(hdc, pf, &pfd)) { MessageBoxW(nullptr, L"No se pudo configurar OpenGL.", L"VR32X Track Editor", MB_ICONERROR); return 1; }
    HGLRC glrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, glrc);
    g_opengl32 = LoadLibraryW(L"opengl32.dll");
    const char *missing = nullptr;
    if (!vrglLoad(getProc, &missing)) {
        std::wstring m = L"Tu tarjeta grafica no ofrece OpenGL 3.3 (falta " + widen(missing) + L").\nActualiza el driver de video.";
        MessageBoxW(nullptr, m.c_str(), L"VR32X Track Editor", MB_ICONERROR);
        return 1;
    }
    ShowWindow(plat.hwnd, SW_SHOWDEFAULT);
    UpdateWindow(plat.hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(scale);
    {
        wchar_t win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH);
        std::string fonts = narrow(win) + "\\Fonts\\";
        std::string a = fonts + "segoeui.ttf", b = fonts + "tahoma.ttf", c = fonts + "arial.ttf";
        loadFont(17.0f * scale, {a.c_str(), b.c_str(), c.c_str()});
    }
    ImGui_ImplWin32_InitForOpenGL(plat.hwnd);
    ImGui_ImplOpenGL3_Init("#version 330");

    App app;
    g_app = &app;
    std::string err;
    if (!app.init(&plat, err)) { MessageBoxW(nullptr, widen(err).c_str(), L"VR32X Track Editor", MB_ICONERROR); return 1; }
    if (!opt.rom.empty()) { app.openRom(opt.rom); app.selectTrack(opt.track - 1); }
    if (opt.view) app.setViewportForTest(opt.yaw, opt.pitch, opt.zoom);
    bool automated = !opt.screenshot.empty();

    for (int frame = 0; !app.quitRequested(); ++frame) {
        MSG msg;
        bool done = false;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        if (IsIconic(plat.hwnd)) { Sleep(10); continue; }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app.frame();
        ImGui::Render();
        glViewport(0, 0, g_width, g_height);
        glClearColor(0.1f, 0.1f, 0.12f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (automated && frame + 1 >= opt.frames) { app.screenshotViewport(opt.screenshot); break; }
        SwapBuffers(hdc);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(glrc);
    ReleaseDC(plat.hwnd, hdc);
    DestroyWindow(plat.hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
