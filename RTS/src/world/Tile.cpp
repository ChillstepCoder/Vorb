#include "stdafx.h"
#include "world/Tile.h"

TileData sTileData[255];

const TileData& getTileData(Tile tile) {
    return sTileData[(int)tile];
}

void initTileData() {
    sTileData[(int)Tile::TILE_STONE_1].hasCollision = true;
}

