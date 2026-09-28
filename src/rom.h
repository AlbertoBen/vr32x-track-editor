// Virtua Racing Deluxe (32X) ROM access: tracks, geometry blocks, palettes.
#pragma once
#include <array>
#include <filesystem>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vr {

// Paths are UTF-8 everywhere (needed for accents in Windows folder names).
inline std::filesystem::path u8path(const std::string &s) { return std::filesystem::u8path(s); }

struct Vec3s { int16_t x, y, z; };

// One polygon of a block. idx[] are vertex indices inside the block.
struct Face {
    uint16_t header = 0;   // high byte: strip kind (0,2,4,6), low byte: render flags
    uint16_t color = 0;    // high byte: palette index, bit 0: triangle
    int n = 3;             // 3 or 4 vertices
    uint16_t idx[4] = {0, 0, 0, 0};
    bool hasNormal = false;
    int16_t normal[3] = {0, 0, 0};
    uint8_t paletteIndex() const { return uint8_t(color >> 8); }
};

// A geometry block: what one grid cell points to.
struct Block {
    uint32_t ptr = 0;      // SH2 address stored in the grid (0x22xxxxxx)
    uint32_t offset = 0;   // file offset
    uint32_t size = 0;     // encoded size in bytes
    uint16_t word1 = 0;    // second header word (meaning unknown, preserved)
    std::vector<Vec3s> verts;
    std::vector<Face> faces;
};

struct Track {
    std::string name;
    uint32_t gridOffset = 0;                // 32x32 table of SH2 pointers
    int palette = 0;                        // index of the 256-colour palette
    std::array<uint32_t, 1024> cells{};     // raw grid entries
    std::map<uint32_t, Block> blocks;       // key: file offset
    const Block *cell(int row, int col) const;
};

constexpr uint32_t kEmptyCell = 0x2207FFFE;

class Rom {
public:
    bool load(const std::string &path, std::string &error);
    bool save(const std::string &path, std::string &error);   // fixes the Sega checksum

    std::vector<Track> tracks;
    std::array<uint32_t, 256> palette(int index) const;         // 0xAARRGGBB

    // Re-encode a block in place (same encoded size or smaller). Returns false with a reason.
    bool writeBlock(Block &block, std::string &error);

    const std::vector<uint8_t> &bytes() const { return data_; }
    std::string path;

private:
    std::vector<uint8_t> data_;
    uint16_t u16(uint32_t off) const { return uint16_t(data_[off] << 8 | data_[off + 1]); }
    int16_t s16(uint32_t off) const { return int16_t(u16(off)); }
    uint32_t u32(uint32_t off) const { return uint32_t(u16(off)) << 16 | u16(off + 2); }
    bool parseBlock(uint32_t ptr, Block &out, std::string &error) const;
};

std::vector<uint8_t> encodeBlock(const Block &block);

} // namespace vr
