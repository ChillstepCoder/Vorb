#pragma once

#include "TileCollision.h"

constexpr ui16 TILE_ID_NONE = UINT16_MAX;
constexpr int TILE_LAYER_GROUND = 0;
constexpr int TILE_LAYER_MID = 1;
constexpr int TILE_LAYER_TOP = 2;
constexpr int TILE_LAYER_COUNT = 3;

enum class TileLayer {
	Ground = 0,
	Mid = 1,
	Top = 2,
	COUNT = 3
};
static_assert(TILE_LAYER_COUNT == enum_cast(TileLayer::COUNT));

enum TileFlags : ui8 {
	TILE_FLAG_IS_INTERACTING = 1 << 0,
	TILE_FLAG_IS_STOCKPILE   = 1 << 1, // True if owned by a stockpile
	TILE_FLAG_IN_CITY        = 1 << 2, // True if inside city limits
	TILE_FLAG_HAS_ITEM_STACK = 1 << 3,
	TILE_FLAG_IS_BUILDING    = 1 << 4,
	TILE_FLAG_TERM           = 1 << 7,
};
static_assert(TILE_FLAG_TERM <= 0x80); // Must fit into a byte

struct Tile {
	Tile() {};
    Tile(TileID ground, TileID mid, TileID top) : groundLayer(ground), midLayer(mid), topLayer(top) { }
    Tile(TileID ground, TileID mid, TileID top, f32 zPos) : groundLayer(ground), midLayer(mid), topLayer(top), baseZPosition(zPos) { }
    Tile(TileID ground, TileID mid, TileID top, f32 zPos, TileFlags flags) : groundLayer(ground), midLayer(mid), topLayer(top), baseZPosition(zPos), tileFlags(flags) { }

    void setTileFlag(TileFlags flag) { tileFlags |= flag; }
    void setTileFlags(TileFlags flags) { tileFlags = flags; }
    void clearTileFlag(TileFlags flag) { tileFlags &= (~flag); }
    void clearTileFlags() { tileFlags = 0; }
	bool hasFlag(TileFlags flag) const { return tileFlags & flag; }
	// TODO: This should be a pointer
	TileCollision buildTileCollision() const;

	union {
		struct {
			TileID groundLayer; // Walls, floors, foundation     // ALWAYS BOX COLLISION
			TileID midLayer;    // Carpet, boards, flora         // NO COLLIDE ONLY
			TileID topLayer;    // Furniture, props, walls trees // ALLOWS CUSTOM COLLISION
		};
		TileID layers[TILE_LAYER_COUNT] = { TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE };
	};
    ui8 tileFlags = 0;
    f32 baseZPosition = 0;
};
static_assert(sizeof(Tile) == 12, "Keep small");

enum class TileShape {
	THIN,  // Trees and flora
    BLOCK, // Most blocks
	// Custom TODO
	COUNT
};
KEG_ENUM_DECL(TileShape);

enum class TileResource {
	NONE,
	WOOD,
	STONE,
	COUNT
};
KEG_ENUM_DECL(TileResource);

struct ItemInputDef {
	nString itemName;
	ui32 count;
};
KEG_TYPE_DECL(ItemInputDef);

struct ItemDrop {
    ItemID id;
    ui32v2 countRange;
};

struct ItemDropDef {
	nString itemName;
	ui32v2 countRange;
};
KEG_TYPE_DECL(ItemDropDef);

