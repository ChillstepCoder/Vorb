#pragma once

#include "terrain/CompressedHeight.h"

struct FloraTileData {
    ui8 age;
    ui8 fruitAge;

    BINARY_SERIALIZE() {
        s.value1b(age);
        s.value1b(fruitAge);
    }
};

// Stairs, furniture, ect
struct OrientedTileData {
    Cartesian orientation;
    CompressedHeight groundZOffset;

    BINARY_SERIALIZE() {
        s.value1b(orientation);
        s.value2b(groundZOffset);
    }
};

// For stairs, ladders, ramp parts, ect
struct Navagable1x1Data {
    ui8 navMask;
};

using TileTypeDataVariant = std::variant<std::monostate, OrientedTileData, FloraTileData/*, Navagable1x1Data*/>;