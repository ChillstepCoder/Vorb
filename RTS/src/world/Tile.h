#pragma once

enum class Tile : ui8 {
	TILE_GRASS_0 = 0,
	TILE_GRASS_1 = 1,
	TILE_STONE_1 = 60,
	TILE_STONE_2 = 70,
	TILE_INVALID = 255
};

struct TileData {
	bool hasCollision = false;
};

const TileData& getTileData(Tile tile);
void initTileData();
