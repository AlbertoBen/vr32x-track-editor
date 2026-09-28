// OBJ/MTL export and import of track geometry.
#pragma once
#include "rom.h"
#include <string>

namespace vr {

// ROM units per OBJ unit. Coordinates are exact multiples of 1/kObjScale.
constexpr double kObjScale = 256.0;

// Writes <path> and the .mtl next to it. One object per block, named cell_RR_CC_OFFSET.
bool exportObj(const Rom &rom, const Track &track, const std::string &path, std::string &error,
               int onlyRow = -1, int onlyCol = -1);

struct ImportReport {
    int blocksChanged = 0, vertsMoved = 0, facesRecoloured = 0;
    std::string log;
};

// Applies an OBJ exported by exportObj (and edited) back to the ROM in memory.
// Current rule: same vertex and face count per block; positions and materials may change.
bool importObj(Rom &rom, Track &track, const std::string &path, ImportReport &report, std::string &error);

} // namespace vr
