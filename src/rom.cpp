#include "rom.h"
#include <cstdio>
#include <fstream>

namespace vr {

// Track layout confirmed on the USA ROM (see docs/FORMAT.md).
struct TrackDef { const char *name; uint32_t grid; int palette; };
static const TrackDef kTracks[] = {
    {"Circuito 1 (Big Forest)", 0x15C000, 0},
    {"Circuito 2", 0x15D000, 1},
    {"Circuito 3", 0x15E000, 2},
    {"Circuito 4", 0x15F000, 3},
    {"Circuito 5", 0x161000, 4},
};
static const uint32_t kPaletteBase = 0x3A238;

const Block *Track::cell(int row, int col) const {
    uint32_t p = cells[row * 32 + col];
    if (p == kEmptyCell) return nullptr;
    auto it = blocks.find(p & 0x3FFFFF);
    return it == blocks.end() ? nullptr : &it->second;
}

bool Rom::parseBlock(uint32_t ptr, Block &b, std::string &error) const {
    uint32_t o = ptr & 0x3FFFFF;
    if ((ptr & 0xDFC00000) != 0x02000000 || o + 4 > data_.size()) { error = "bad block pointer"; return false; }
    b.ptr = ptr; b.offset = o;
    uint16_t nv = u16(o); b.word1 = u16(o + 2);
    uint32_t p = o + 4;
    if (nv == 0 || nv > 512 || p + 6u * nv > data_.size()) { error = "bad vertex count"; return false; }
    b.verts.resize(nv);
    for (auto &v : b.verts) { v = {s16(p), s16(p + 2), s16(p + 4)}; p += 6; }
    uint16_t slot[4] = {0, 0, 0, 0};
    for (int guard = 0;; ++guard) {
        if (guard > 4096 || p + 2 > data_.size()) { error = "unterminated face list"; return false; }
        uint16_t h = u16(p); p += 2;
        int kind = (h >> 8) & 14;
        if (kind == 12) break;
        Face f; f.header = h; f.color = u16(p); p += 2;
        if (kind == 0) { slot[0] = u16(p) >> 4; slot[1] = u16(p + 2) >> 4; p += 4; }
        else if (kind == 2) slot[1] = slot[3];
        else if (kind == 4) { slot[0] = slot[3]; slot[1] = slot[2]; }
        else if (kind == 6) slot[0] = slot[2];
        else { error = "unknown face kind"; return false; }
        slot[2] = u16(p) >> 4; p += 2;
        if (f.color & 1) { slot[3] = slot[2]; f.n = 3; }
        else { slot[3] = u16(p) >> 4; p += 2; f.n = 4; }
        if (h & 0x20) { f.hasNormal = true; for (int k = 0; k < 3; ++k) f.normal[k] = s16(p + 2 * k); p += 6; }
        for (int k = 0; k < 4; ++k) {
            f.idx[k] = slot[k];
            if (slot[k] >= nv) { error = "face index out of range"; return false; }
        }
        b.faces.push_back(f);
    }
    b.size = p - o;
    return true;
}

bool Rom::load(const std::string &file, std::string &error) {
    std::ifstream in(u8path(file), std::ios::binary);
    if (!in) { error = "no se puede abrir " + file; return false; }
    data_.assign(std::istreambuf_iterator<char>(in), {});
    if (data_.size() < 0x300000) { error = "no parece una ROM de Virtua Racing Deluxe (tamaño)"; return false; }
    tracks.clear();
    for (const auto &d : kTracks) {
        Track t; t.name = d.name; t.gridOffset = d.grid; t.palette = d.palette;
        for (int i = 0; i < 1024; ++i) {
            uint32_t p = u32(d.grid + 4 * i);
            t.cells[i] = p;
            if (p == kEmptyCell || t.blocks.count(p & 0x3FFFFF)) continue;
            Block b;
            if (!parseBlock(p, b, error)) {
                char buf[128]; std::snprintf(buf, sizeof buf, "%s, celda %d: ", d.name, i);
                error = buf + error + " (no parece Virtua Racing Deluxe)"; return false;
            }
            t.blocks[b.offset] = std::move(b);
        }
        tracks.push_back(std::move(t));
    }
    path = file;
    return true;
}

std::array<uint32_t, 256> Rom::palette(int index) const {
    std::array<uint32_t, 256> pal{};
    for (int i = 0; i < 256; ++i) {
        uint16_t v = u16(kPaletteBase + 0x200 * index + 2 * i);
        uint32_t r = (v & 31) * 255 / 31, g = ((v >> 5) & 31) * 255 / 31, b = ((v >> 10) & 31) * 255 / 31;
        pal[i] = 0xFF000000u | r << 16 | g << 8 | b;
    }
    return pal;
}

// Re-encode a block. Each face uses the shortest record the strip state allows
// (kinds 2/4/6 reuse two vertices of the previous face), so an unchanged block
// encodes to the same size as the original.
std::vector<uint8_t> encodeBlock(const Block &b) {
    std::vector<uint8_t> o;
    auto w16 = [&](uint16_t v) { o.push_back(uint8_t(v >> 8)); o.push_back(uint8_t(v)); };
    w16(uint16_t(b.verts.size())); w16(b.word1);
    for (auto &v : b.verts) { w16(uint16_t(v.x)); w16(uint16_t(v.y)); w16(uint16_t(v.z)); }
    uint16_t slot[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    for (auto &f : b.faces) {
        int kind = 0;
        if (slot[0] != 0xFFFF) {
            if (f.idx[0] == slot[0] && f.idx[1] == slot[3]) kind = 2;
            else if (f.idx[0] == slot[3] && f.idx[1] == slot[2]) kind = 4;
            else if (f.idx[0] == slot[2] && f.idx[1] == slot[1]) kind = 6;
        }
        bool tri = f.n == 3;
        uint16_t color = uint16_t((f.color & ~1u) | (tri ? 1 : 0));
        uint16_t header = uint16_t((f.header & ~0x0E21u) | (kind << 8) | (tri ? 1 : 0) | (f.hasNormal ? 0x20 : 0));
        w16(header); w16(color);
        if (kind == 0) { w16(uint16_t(f.idx[0] << 4)); w16(uint16_t(f.idx[1] << 4)); }
        w16(uint16_t(f.idx[2] << 4));
        if (!tri) w16(uint16_t(f.idx[3] << 4));
        if (f.hasNormal) for (int k = 0; k < 3; ++k) w16(uint16_t(f.normal[k]));
        slot[0] = f.idx[0]; slot[1] = f.idx[1]; slot[2] = f.idx[2]; slot[3] = tri ? f.idx[2] : f.idx[3];
    }
    w16(0x0C00);
    return o;
}

bool Rom::writeBlock(Block &b, std::string &error) {
    std::vector<uint8_t> enc = encodeBlock(b);
    if (enc.size() > b.size) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "el bloque en %06X ocupa %zu bytes y solo caben %u", b.offset, enc.size(), b.size);
        error = buf; return false;
    }
    std::copy(enc.begin(), enc.end(), data_.begin() + b.offset);
    for (uint32_t i = b.offset + uint32_t(enc.size()); i < b.offset + b.size; ++i) data_[i] = 0;  // unused tail
    return true;
}

bool Rom::save(const std::string &file, std::string &error) {
    uint32_t sum = 0;
    for (size_t i = 0x200; i + 1 < data_.size(); i += 2) sum += u16(uint32_t(i));
    data_[0x18E] = uint8_t(sum >> 8); data_[0x18F] = uint8_t(sum);
    std::ofstream out(u8path(file), std::ios::binary);
    if (!out) { error = "no se puede escribir " + file; return false; }
    out.write(reinterpret_cast<const char *>(data_.data()), std::streamsize(data_.size()));
    return bool(out);
}

} // namespace vr
