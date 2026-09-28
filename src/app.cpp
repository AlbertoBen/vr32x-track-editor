#include "app.h"
#include "gl.h"
#include "imgui.h"
#include "objio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

static const char *kVersion = "0.1";

bool App::init(Platform *platform, std::string &error) {
    platform_ = platform;
    const char *err = nullptr;
    if (!renderer_.init(&err)) { error = std::string("OpenGL: ") + (err ? err : "?"); return false; }
    ImGuiStyle &st = ImGui::GetStyle();
    st.WindowRounding = 4; st.FrameRounding = 3; st.GrabRounding = 3;
    status_ = "Abre una ROM de Virtua Racing Deluxe (Archivo > Abrir ROM).";
    updateTitle();
    return true;
}

void App::updateTitle() {
    std::string t = "VR32X Track Editor";
    if (romLoaded_) {
        std::string name = rom_.path.substr(rom_.path.find_last_of("/\\") + 1);
        t += " - " + name + (romDirty_ ? " *" : "");
    }
    platform_->setTitle(t);
}

bool App::openRom(const std::string &path) {
    vr::Rom r;
    std::string err;
    if (!r.load(path, err)) { status_ = "Error: " + err; return false; }
    rom_ = std::move(r);
    romLoaded_ = true; romDirty_ = false;
    selectTrack(0);
    status_ = "ROM cargada: " + std::to_string(rom_.tracks.size()) + " circuitos.";
    updateTitle();
    return true;
}

void App::selectTrack(int index) {
    if (!romLoaded_ || index < 0 || index >= int(rom_.tracks.size())) return;
    track_ = index; selected_ = hover_ = -1;
    rebuild(true);
}

void App::rebuild(bool reframe) {
    const vr::Track &t = rom_.tracks[track_];
    blockKeys_.clear();
    for (auto &kv : t.blocks) blockKeys_.push_back(kv.first);
    renderer_.setTrack(t, rom_.palette(t.palette));
    if (reframe) {
        camera_.frame(renderer_.boundsMin(), renderer_.boundsMax());
        camera_.yaw = 0.0f; camera_.pitch = 1.0f;
    }
}

int App::blockIdAt(int row, int col) const {
    const vr::Track &t = rom_.tracks[track_];
    uint32_t p = t.cells[row * 32 + col];
    if (p == vr::kEmptyCell) return -1;
    auto it = std::lower_bound(blockKeys_.begin(), blockKeys_.end(), p & 0x3FFFFF);
    return (it != blockKeys_.end() && *it == (p & 0x3FFFFF)) ? int(it - blockKeys_.begin()) : -1;
}

bool App::cellOfBlock(int id, int &row, int &col) const {
    if (id < 0) return false;
    for (int i = 0; i < 1024; ++i)
        if (blockIdAt(i / 32, i % 32) == id) { row = i / 32; col = i % 32; return true; }
    return false;
}

void App::frame() {
    hover_ = -1;
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    mainMenu();
    float statusH = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##left", ImVec2(330, -statusH), ImGuiChildFlags_Borders);
    leftPanel();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##view", ImVec2(0, -statusH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    viewport();
    ImGui::EndChild();
    ImGui::TextUnformatted(status_.c_str());
    ImGui::End();
    if (showPalette_) paletteWindow();
    if (showHelp_) helpWindow();
    if (showAbout_) {
        ImGui::Begin("Acerca de", &showAbout_, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("VR32X Track Editor %s", kVersion);
        ImGui::TextUnformatted("Visor y editor de circuitos de Virtua Racing Deluxe (32X).");
        ImGui::TextUnformatted("Dear ImGui © Omar Cornut, licencia MIT.");
        ImGui::End();
    }
}

void App::mainMenu() {
    if (!ImGui::BeginMenuBar()) return;
    if (ImGui::BeginMenu("Archivo")) {
        if (ImGui::MenuItem("Abrir ROM...", "Ctrl+O")) {
            std::string p;
            if (platform_->openFile("Abrir ROM", "ROM 32X|*.32x;*.bin|Todos|*.*", p)) openRom(p);
        }
        if (ImGui::MenuItem("Guardar ROM como...", "Ctrl+S", false, romLoaded_)) saveRom();
        ImGui::Separator();
        if (ImGui::MenuItem("Exportar circuito a OBJ...", nullptr, false, romLoaded_)) exportTrack(false);
        if (ImGui::MenuItem("Exportar bloque seleccionado a OBJ...", nullptr, false, romLoaded_ && selected_ >= 0)) exportTrack(true);
        if (ImGui::MenuItem("Importar OBJ...", nullptr, false, romLoaded_)) importObjFile();
        ImGui::Separator();
        if (ImGui::MenuItem("Salir")) quit_ = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Ver")) {
        ImGui::MenuItem("Iluminación", nullptr, &options_.lighting);
        ImGui::MenuItem("Aristas", nullptr, &options_.wireframe);
        ImGui::MenuItem("Rejilla de celdas", nullptr, &options_.cellGrid);
        ImGui::MenuItem("Paleta de colores", nullptr, &showPalette_);
        ImGui::Separator();
        if (ImGui::MenuItem("Encuadrar circuito", "Inicio", false, romLoaded_)) { camera_.frame(renderer_.boundsMin(), renderer_.boundsMax()); }
        if (ImGui::MenuItem("Vista superior", "T", false, romLoaded_)) { camera_.pitch = 1.5607f; camera_.yaw = 0; }
        ImGui::ColorEdit3("Fondo", options_.background, ImGuiColorEditFlags_NoInputs);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Ayuda")) {
        ImGui::MenuItem("Controles y formato", nullptr, &showHelp_);
        ImGui::MenuItem("Acerca de", nullptr, &showAbout_);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    ImGuiIO &io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        std::string p;
        if (platform_->openFile("Abrir ROM", "ROM 32X|*.32x;*.bin|Todos|*.*", p)) openRom(p);
    }
    if (romLoaded_ && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) saveRom();
}

void App::leftPanel() {
    if (!romLoaded_) {
        ImGui::TextWrapped("No hay ninguna ROM abierta.");
        if (ImGui::Button("Abrir ROM...", ImVec2(-1, 0))) {
            std::string p;
            if (platform_->openFile("Abrir ROM", "ROM 32X|*.32x;*.bin|Todos|*.*", p)) openRom(p);
        }
        ImGui::Spacing();
        ImGui::TextWrapped("Sirve la ROM original de Virtua Racing Deluxe (USA) y las versiones a 30 FPS.");
        return;
    }
    ImGui::SeparatorText("Circuito");
    for (int i = 0; i < int(rom_.tracks.size()); ++i)
        if (ImGui::RadioButton(rom_.tracks[i].name.c_str(), track_ == i)) selectTrack(i);
    ImGui::SeparatorText("Mapa de bloques (32 x 32)");
    cellMap();
    ImGui::SeparatorText("Bloque seleccionado");
    blockInfo();
    ImGui::SeparatorText("OBJ");
    if (ImGui::Button("Exportar circuito", ImVec2(155, 0))) exportTrack(false);
    ImGui::SameLine();
    ImGui::BeginDisabled(selected_ < 0);
    if (ImGui::Button("Exportar bloque", ImVec2(-1, 0))) exportTrack(true);
    ImGui::EndDisabled();
    if (ImGui::Button("Importar OBJ...", ImVec2(155, 0))) importObjFile();
    ImGui::SameLine();
    if (ImGui::Button(romDirty_ ? "Guardar ROM *" : "Guardar ROM", ImVec2(-1, 0))) saveRom();
}

void App::cellMap() {
    const vr::Track &t = rom_.tracks[track_];
    auto pal = rom_.palette(t.palette);
    float size = std::floor(std::min(ImGui::GetContentRegionAvail().x, 300.0f) / 32.0f);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##map", ImVec2(size * 32, size * 32));
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + size * 32, p0.y + size * 32), IM_COL32(25, 25, 30, 255));
    int hr = -1, hc = -1;
    if (ImGui::IsItemHovered()) {
        ImVec2 m = ImGui::GetIO().MousePos;
        hr = int((m.y - p0.y) / size); hc = int((m.x - p0.x) / size);
    }
    int sr = -1, sc = -1;
    cellOfBlock(selected_, sr, sc);
    for (int r = 0; r < 32; ++r)
        for (int c = 0; c < 32; ++c) {
            const vr::Block *b = t.cell(r, c);
            if (!b) continue;
            // average colour of the block's faces
            unsigned R = 0, G = 0, B = 0;
            for (auto &f : b->faces) { uint32_t col = pal[f.paletteIndex()]; R += (col >> 16) & 255; G += (col >> 8) & 255; B += col & 255; }
            size_t n = std::max<size_t>(1, b->faces.size());
            ImU32 col = IM_COL32(R / n, G / n, B / n, 255);
            ImVec2 a(p0.x + c * size, p0.y + r * size), z(a.x + size - 1, a.y + size - 1);
            dl->AddRectFilled(a, z, col);
            if (r == sr && c == sc) dl->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(z.x + 1, z.y + 1), IM_COL32(255, 220, 30, 255), 0, 0, 2);
        }
    if (hr >= 0 && hr < 32 && hc >= 0 && hc < 32) {
        ImVec2 a(p0.x + hc * size, p0.y + hr * size);
        dl->AddRect(a, ImVec2(a.x + size, a.y + size), IM_COL32(255, 255, 255, 255));
        int id = blockIdAt(hr, hc);
        hover_ = id;
        ImGui::SetTooltip("Celda %d, %d%s", hr, hc, id < 0 ? " (vacía)" : "");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selected_ = id;
            if (id >= 0) camera_.target = renderer_.blockCenter(id);
        }
        if (id >= 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            camera_.target = renderer_.blockCenter(id);
            camera_.distance = 25.0f;
        }
    }
    ImGui::TextDisabled("Clic: seleccionar.  Doble clic: acercar.");
}

void App::blockInfo() {
    if (selected_ < 0) { ImGui::TextDisabled("Ninguno. Haz clic en el mapa o en la vista 3D."); return; }
    const vr::Block &b = rom_.tracks[track_].blocks.at(blockKeys_[selected_]);
    int r = -1, c = -1;
    cellOfBlock(selected_, r, c);
    ImGui::Text("Celda %d, %d   ROM %06X", r, c, b.offset);
    ImGui::Text("%zu vértices, %zu caras, %u bytes", b.verts.size(), b.faces.size(), b.size);
    int tris = 0;
    for (auto &f : b.faces) tris += f.n == 3;
    ImGui::Text("%d triángulos, %zu cuadriláteros", tris, b.faces.size() - tris);
}

void App::viewport() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int w = std::max(1, int(avail.x)), h = std::max(1, int(avail.y));
    viewW_ = w; viewH_ = h;
    if (!romLoaded_) {
        ImGui::SetCursorPos(ImVec2(avail.x * 0.5f - 150, avail.y * 0.5f));
        ImGui::TextDisabled("Archivo > Abrir ROM... para empezar");
        return;
    }
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    renderer_.render(w, h, camera_, options_, selected_, hover_);
    ImGui::Image((ImTextureID)(intptr_t)renderer_.texture(), ImVec2(float(w), float(h)), ImVec2(0, 1), ImVec2(1, 0));
    bool hovered = ImGui::IsItemHovered();
    if (!hovered) return;
    hover_ = -1;
    ImVec2 m(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    // camera: right drag = orbit, middle drag or shift+right = pan, wheel = zoom
    bool pan = ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || (io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Right));
    if (pan) {
        Vec3 eye = camera_.eye(), f = normalize(camera_.target - eye);
        Vec3 s = normalize(cross(f, Vec3{0, 1, 0})), u = cross(s, f);
        float k = camera_.distance * 0.0015f;
        camera_.target = camera_.target - s * (io.MouseDelta.x * k) + u * (io.MouseDelta.y * k);
    } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        camera_.yaw -= io.MouseDelta.x * 0.006f;
        camera_.pitch = std::clamp(camera_.pitch + io.MouseDelta.y * 0.006f, -1.55f, 1.5607f);
    }
    if (io.MouseWheel != 0) camera_.distance = std::clamp(camera_.distance * std::pow(0.87f, io.MouseWheel), 0.5f, 4000.0f);
    // keyboard: WASD move the target on the ground plane, Q/E up and down
    Vec3 fwd = normalize(Vec3{-std::sin(camera_.yaw), 0, -std::cos(camera_.yaw)}), right{-fwd.z, 0, fwd.x};
    float step = camera_.distance * 0.9f * io.DeltaTime * (io.KeyShift ? 3.0f : 1.0f);
    if (ImGui::IsKeyDown(ImGuiKey_W)) camera_.target = camera_.target + fwd * step;
    if (ImGui::IsKeyDown(ImGuiKey_S) && !io.KeyCtrl) camera_.target = camera_.target - fwd * step;
    if (ImGui::IsKeyDown(ImGuiKey_D)) camera_.target = camera_.target + right * step;
    if (ImGui::IsKeyDown(ImGuiKey_A)) camera_.target = camera_.target - right * step;
    if (ImGui::IsKeyDown(ImGuiKey_E)) camera_.target.y += step;
    if (ImGui::IsKeyDown(ImGuiKey_Q)) camera_.target.y -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_T, false)) { camera_.pitch = 1.5607f; camera_.yaw = 0; }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) camera_.frame(renderer_.boundsMin(), renderer_.boundsMax());
    if (ImGui::IsKeyPressed(ImGuiKey_F, false) && selected_ >= 0) { camera_.target = renderer_.blockCenter(selected_); camera_.distance = 25; }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        PickResult pr = renderer_.pick(camera_, w, h, m.x, m.y);
        hover_ = pr.hit ? pr.blockId : -1;
        if (pr.hit) {
            int r, c;
            if (cellOfBlock(pr.blockId, r, c)) ImGui::SetTooltip("Celda %d, %d", r, c);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) selected_ = hover_;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && pr.hit) { camera_.target = pr.point; }
    }
}

void App::paletteWindow() {
    ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Paleta del circuito", &showPalette_)) { ImGui::End(); return; }
    if (!romLoaded_) { ImGui::TextDisabled("Sin ROM"); ImGui::End(); return; }
    ImGui::TextWrapped("En el OBJ, el material cXX es el color XX (hexadecimal) de esta paleta.");
    auto pal = rom_.palette(rom_.tracks[track_].palette);
    for (int i = 0; i < 256; ++i) {
        uint32_t c = pal[i];
        ImVec4 col(((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, 1);
        char id[16]; std::snprintf(id, sizeof id, "c%02X", i);
        ImGui::ColorButton(id, col, ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("c%02X", i);
        if (i % 16 != 15) ImGui::SameLine(0, 2);
    }
    ImGui::End();
}

void App::helpWindow() {
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Controles y formato", &showHelp_)) { ImGui::End(); return; }
    ImGui::SeparatorText("Vista 3D");
    ImGui::BulletText("Botón derecho + arrastrar: girar.");
    ImGui::BulletText("Botón central o Mayús + derecho: desplazar.");
    ImGui::BulletText("Rueda: acercar / alejar.   W A S D: moverse.   Q / E: bajar / subir.");
    ImGui::BulletText("Clic: seleccionar bloque.   F: centrar el seleccionado.   T: vista superior.   Inicio: todo el circuito.");
    ImGui::SeparatorText("Cómo está hecho un circuito");
    ImGui::TextWrapped("Cada circuito es una rejilla de 32 x 32 celdas. Cada celda apunta a un bloque de polígonos "
                       "(triángulos y cuadriláteros de un solo color). El juego solo dibuja la celda donde está la "
                       "cámara y sus vecinas, así que la geometría de un bloque debe quedarse cerca de su celda.");
    ImGui::SeparatorText("Editar con OBJ");
    ImGui::TextWrapped("1. Exporta el circuito (o un bloque).\n"
                       "2. Edítalo en Blender u otro programa: mueve vértices, cambia materiales (cXX = color de la paleta), "
                       "añade o quita caras. No cambies los nombres de los objetos (cell_FILA_COLUMNA_OFFSET).\n"
                       "3. Importa el OBJ y guarda la ROM con otro nombre.\n\n"
                       "Límites de esta versión: cada bloque tiene que caber en el hueco que ocupaba en la ROM; "
                       "la física y las colisiones usan otros datos, así que cambiar la forma de la carretera no cambia "
                       "por dónde circula el coche.");
    ImGui::End();
}

void App::exportTrack(bool onlySelected) {
    const vr::Track &t = rom_.tracks[track_];
    int r = -1, c = -1;
    if (onlySelected && !cellOfBlock(selected_, r, c)) return;
    char def[64];
    if (onlySelected) std::snprintf(def, sizeof def, "circuito%d_celda_%02d_%02d.obj", track_ + 1, r, c);
    else std::snprintf(def, sizeof def, "circuito%d.obj", track_ + 1);
    std::string path = def;
    if (!platform_->saveFile("Exportar OBJ", "Wavefront OBJ|*.obj", "obj", path)) return;
    std::string err;
    if (vr::exportObj(rom_, t, path, err, r, c)) status_ = "Exportado: " + path;
    else status_ = "Error: " + err;
}

void App::importObjFile() {
    std::string path;
    if (!platform_->openFile("Importar OBJ", "Wavefront OBJ|*.obj", path)) return;
    vr::ImportReport rep;
    std::string err;
    if (!vr::importObj(rom_, rom_.tracks[track_], path, rep, err)) { status_ = "Error al importar: " + err; return; }
    if (rep.blocksChanged == 0) { status_ = "Importado: no había cambios respecto a la ROM."; return; }
    romDirty_ = true;
    rebuild(false);
    char buf[160];
    std::snprintf(buf, sizeof buf, "Importado: %d bloques cambiados (%d vértices, %d caras recoloreadas). Guarda la ROM para conservarlo.",
                  rep.blocksChanged, rep.vertsMoved, rep.facesRecoloured);
    status_ = buf;
    updateTitle();
}

void App::saveRom() {
    std::string path = rom_.path;
    size_t dot = path.find_last_of('.');
    path = (dot == std::string::npos ? path : path.substr(0, dot)) + "_editada.32x";
    if (!platform_->saveFile("Guardar ROM como", "ROM 32X|*.32x", "32x", path)) return;
    std::string err;
    if (!rom_.save(path, err)) { status_ = "Error: " + err; return; }
    rom_.path = path;
    romDirty_ = false;
    status_ = "ROM guardada: " + path;
    updateTitle();
}

bool App::screenshotViewport(const std::string &png) {
    int w, h;
    auto px = renderer_.readPixels(w, h);
    return w > 0 && stbi_write_png(png.c_str(), w, h, 4, px.data(), w * 4);
}

void App::setViewportForTest(float yaw, float pitch, float zoom) {
    camera_.yaw = yaw; camera_.pitch = pitch; camera_.distance *= zoom;
}
