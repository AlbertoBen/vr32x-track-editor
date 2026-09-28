#include "objio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace vr {

static std::string baseName(const std::string &p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

// OBJ axes: x = east, y = up, z = south (ROM z points north).
static void romToObj(const Vec3s &v, double out[3]) {
    out[0] = v.x / kObjScale; out[1] = v.y / kObjScale; out[2] = -v.z / kObjScale;
}

bool exportObj(const Rom &rom, const Track &t, const std::string &path, std::string &error,
               int onlyRow, int onlyCol) {
    std::string mtlPath = path.substr(0, path.find_last_of('.')) + ".mtl";
    std::ofstream obj(u8path(path)), mtl(u8path(mtlPath));
    if (!obj || !mtl) { error = "no se puede escribir " + path; return false; }
    auto pal = rom.palette(t.palette);
    obj << "# Virtua Racing Deluxe 32X - " << t.name << "\n"
        << "# Exportado con vr32x-track-editor. Unidades: 1 = " << kObjScale << " unidades de ROM.\n"
        << "# Cada objeto es un bloque de la cuadricula: cell_FILA_COLUMNA_OFFSET. No cambies los nombres.\n"
        << "# Materiales cXX = indice XX de la paleta del circuito.\n"
        << "mtllib " << baseName(mtlPath) << "\n";
    std::set<int> used;
    std::set<uint32_t> done;
    int base = 1;
    char buf[160];
    for (int i = 0; i < 1024; ++i) {
        int r = i / 32, c = i % 32;
        if (onlyRow >= 0 && (r != onlyRow || c != onlyCol)) continue;
        const Block *b = t.cell(r, c);
        if (!b || done.count(b->offset)) continue;
        done.insert(b->offset);
        std::snprintf(buf, sizeof buf, "o cell_%02d_%02d_%06X\n", r, c, b->offset);
        obj << buf;
        for (auto &v : b->verts) {
            double p[3]; romToObj(v, p);
            std::snprintf(buf, sizeof buf, "v %.8g %.8g %.8g\n", p[0], p[1], p[2]);
            obj << buf;
        }
        int cur = -1;
        for (auto &f : b->faces) {
            int ci = f.paletteIndex();
            used.insert(ci);
            if (ci != cur) { std::snprintf(buf, sizeof buf, "usemtl c%02X\n", ci); obj << buf; cur = ci; }
            obj << "f";
            for (int k = 0; k < f.n; ++k) obj << ' ' << base + f.idx[k];
            obj << "\n";
        }
        base += int(b->verts.size());
    }
    for (int ci : used) {
        uint32_t c = pal[ci];
        std::snprintf(buf, sizeof buf, "newmtl c%02X\nKd %.4f %.4f %.4f\n\n", ci,
                      ((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0);
        mtl << buf;
    }
    return true;
}

namespace {
struct ObjFace { std::vector<int> v; int material; };
struct ObjObject { std::string name; std::vector<std::array<double, 3>> verts; std::vector<ObjFace> faces; };
}

static bool parseObj(const std::string &path, std::vector<ObjObject> &objs, std::string &error) {
    std::ifstream in(u8path(path));
    if (!in) { error = "no se puede abrir " + path; return false; }
    std::vector<std::array<double, 3>> all;      // global vertex list
    std::vector<std::pair<int, int>> owner;      // vertex -> (object, local index)
    int material = -1;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::istringstream ss(line);
        std::string tag; ss >> tag;
        if (tag == "o" || tag == "g") {
            std::string name; ss >> name;
            if (objs.empty() || objs.back().name != name) objs.push_back({name, {}, {}});
        } else if (tag == "v") {
            std::array<double, 3> p{}; ss >> p[0] >> p[1] >> p[2];
            if (objs.empty()) objs.push_back({"(sin nombre)", {}, {}});
            all.push_back(p);
            owner.push_back({int(objs.size()) - 1, int(objs.back().verts.size())});
            objs.back().verts.push_back(p);
        } else if (tag == "usemtl") {
            std::string m; ss >> m;
            material = (m.size() == 3 && (m[0] == 'c' || m[0] == 'C')) ? int(std::strtol(m.c_str() + 1, nullptr, 16)) : -1;
        } else if (tag == "f") {
            ObjFace f; f.material = material;
            std::string tok;
            int obj = -1;
            while (ss >> tok) {
                int gi = std::atoi(tok.c_str());
                gi = gi < 0 ? int(all.size()) + gi : gi - 1;
                if (gi < 0 || gi >= int(all.size())) { error = "índice de vértice inválido en la linea " + std::to_string(lineNo); return false; }
                if (obj < 0) obj = owner[gi].first;
                if (owner[gi].first != obj) { error = "una cara usa vértices de dos bloques (linea " + std::to_string(lineNo) + ")"; return false; }
                f.v.push_back(owner[gi].second);
            }
            if (f.v.size() < 3 || f.v.size() > 4) { error = "solo se admiten triángulos y cuadriláteros (linea " + std::to_string(lineNo) + ")"; return false; }
            if (obj >= 0) objs[obj].faces.push_back(f);
        }
    }
    return true;
}

bool importObj(Rom &rom, Track &t, const std::string &path, ImportReport &rep, std::string &error) {
    std::vector<ObjObject> objs;
    if (!parseObj(path, objs, error)) return false;
    char buf[256];
    // Validate everything first, then write, so a bad file changes nothing.
    std::vector<Block> updated;
    for (auto &o : objs) {
        if (o.verts.empty()) continue;
        int r, c; unsigned off;
        if (std::sscanf(o.name.c_str(), "cell_%d_%d_%X", &r, &c, &off) != 3) {
            error = "objeto '" + o.name + "': el nombre debe ser cell_FILA_COLUMNA_OFFSET (el que puso el exportador)";
            return false;
        }
        auto it = t.blocks.find(off);
        if (it == t.blocks.end()) { error = "objeto '" + o.name + "': ese bloque no existe en este circuito"; return false; }
        const Block &orig = it->second;
        Block nb = orig;
        nb.verts.clear();
        for (auto &p : o.verts) {
            double x = std::round(p[0] * kObjScale), y = std::round(p[1] * kObjScale), z = std::round(-p[2] * kObjScale);
            if (std::fabs(x) > 32767 || std::fabs(y) > 32767 || std::fabs(z) > 32767) {
                error = "objeto '" + o.name + "': vértice fuera del rango de la ROM"; return false;
            }
            nb.verts.push_back({int16_t(x), int16_t(y), int16_t(z)});
        }
        if (nb.verts.size() > 256) { error = "objeto '" + o.name + "': más de 256 vértices"; return false; }
        // Faces: keep the original record (flags, normal) when the same vertices are used.
        std::map<std::vector<int>, const Face *> byVerts;
        std::map<uint16_t, int> headerCount;
        for (auto &f : orig.faces) {
            std::vector<int> key(f.idx, f.idx + f.n); std::sort(key.begin(), key.end());
            byVerts[key] = &f;
            headerCount[f.header & 0x00FF]++;
        }
        uint16_t commonHeader = 0x11;
        int best = -1;
        for (auto &h : headerCount) if (h.second > best) { best = h.second; commonHeader = h.first; }
        nb.faces.clear();
        for (auto &of : o.faces) {
            Face f;
            std::vector<int> key(of.v.begin(), of.v.end()); std::sort(key.begin(), key.end());
            auto m = byVerts.find(key);
            if (m != byVerts.end()) f = *m->second;
            else { f.header = commonHeader; f.hasNormal = false; }
            f.n = int(of.v.size());
            for (int k = 0; k < 4; ++k) f.idx[k] = uint16_t(of.v[std::min<size_t>(k, of.v.size() - 1)]);
            if (of.material >= 0) f.color = uint16_t((of.material << 8) | (f.color & 0xFE));
            nb.faces.push_back(f);
        }
        if (nb.faces.empty()) { error = "objeto '" + o.name + "': no tiene caras"; return false; }
        size_t need = encodeBlock(nb).size();
        if (need > orig.size) {
            std::snprintf(buf, sizeof buf, "objeto '%s': ocupa %zu bytes y el hueco original es de %u. "
                          "De momento el bloque no puede crecer.", o.name.c_str(), need, orig.size);
            error = buf; return false;
        }
        int moved = 0, recol = 0;
        for (size_t i = 0; i < nb.verts.size() && i < orig.verts.size(); ++i) {
            auto &a = nb.verts[i]; auto &b = orig.verts[i];
            if (a.x != b.x || a.y != b.y || a.z != b.z) ++moved;
        }
        moved += int(std::max(nb.verts.size(), orig.verts.size()) - std::min(nb.verts.size(), orig.verts.size()));
        for (size_t i = 0; i < nb.faces.size() && i < orig.faces.size(); ++i)
            if (nb.faces[i].paletteIndex() != orig.faces[i].paletteIndex()) ++recol;
        bool sameFaces = nb.faces.size() == orig.faces.size();
        for (size_t i = 0; sameFaces && i < nb.faces.size(); ++i)
            sameFaces = nb.faces[i].n == orig.faces[i].n && std::equal(nb.faces[i].idx, nb.faces[i].idx + 4, orig.faces[i].idx);
        if (!moved && !recol && sameFaces) continue;
        rep.vertsMoved += moved; rep.facesRecoloured += recol;
        std::snprintf(buf, sizeof buf, "%s: %d vértices cambiados, %d caras recoloreadas%s, %zu/%u bytes\n",
                      o.name.c_str(), moved, recol, sameFaces ? "" : ", caras cambiadas", need, orig.size);
        rep.log += buf;
        updated.push_back(std::move(nb));
    }
    for (auto &nb : updated) {
        if (!rom.writeBlock(nb, error)) return false;
        Block &dst = t.blocks[nb.offset];
        uint32_t keepSize = dst.size;
        dst = nb;
        dst.size = keepSize;       // the slot keeps its original capacity
        rep.blocksChanged++;
    }
    return true;
}

} // namespace vr
