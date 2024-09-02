#pragma once

struct FloraTileData {
    ui8 age;
    ui8 fruitAge;

    BINARY_SERIALIZE() {
        s.value1b(age);
        s.value1b(fruitAge);
    }
};

// For stairs, ladders, ramp parts, ect
struct Navagable1x1Data {
    ui8 navMask;
};

using TileTypeDataVariant = std::variant<std::monostate, FloraTileData/*, Navagable1x1Data*/>;