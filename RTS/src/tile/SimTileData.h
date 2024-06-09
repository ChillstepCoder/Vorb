#pragma once

enum class SimTileDataFlags : ui8 {
    Reserved = BIT(0),
    Blocking = BIT(1),
};

struct SimTileData {
    TileID tileId = TILE_ID_NONE;
    ui8 variant = 0;
    BitFlags<SimTileDataFlags> flags;

    auto operator<=>(const SimTileData&) const = default;
    bool isNull() const { return tileId == TILE_ID_NONE && flags.getBits() == 0; }
};
static_assert(sizeof(SimTileData) == 4);