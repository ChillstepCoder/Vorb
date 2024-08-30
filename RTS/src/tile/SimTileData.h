#pragma once

#include "tile/TileTypeDataVariant.h"

enum class SimTileDataFlags : ui8 {
    Reserved = BIT(0),
    Blocking = BIT(1)
};

struct SimTileData {
    TileID tileId = TILE_ID_NONE;
    ui8 variant = 0;
    BitFlags<SimTileDataFlags> flags;
    TileTypeDataVariant typeData;
    static_assert(e_count(TileType) == 2, "Tile type data can be stored in typeData");
    //ui8 padding; // TODO: use for something?

    auto operator<=>(const SimTileData&) const = default;
    bool isNull() const { return tileId == TILE_ID_NONE && flags.getBits() == 0; }
};
static_assert(sizeof(SimTileData) == 8);