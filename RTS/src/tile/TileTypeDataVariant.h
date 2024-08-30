#pragma once

struct FloraTileData {
    ui8 age;
    ui8 fruitAge;

    BINARY_SERIALIZE() {
        s.value1b(age);
        s.value1b(fruitAge);
    }
};

using TileTypeDataVariant = std::variant<std::monostate, FloraTileData>;