#pragma once

#include "rendering/SpriteData.h"

#include "TileCollisionShape.h"

constexpr ui16 TILE_ID_NONE = UINT16_MAX;
typedef ui16 TileID;
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
};

struct Tile {
	Tile() {};
    Tile(TileID ground, TileID mid, TileID top) : groundLayer(ground), midLayer(mid), topLayer(top) { }
    Tile(TileID ground, TileID mid, TileID top, ui16 zPos) : groundLayer(ground), midLayer(mid), topLayer(top), baseZPosition(zPos){ }

	union {
		struct {
			TileID groundLayer; // Dirt, foundation, earth
			TileID midLayer; // Carpet, boards, walls, flora
			TileID topLayer; // Furniture, props, trees
		};
		TileID layers[TILE_LAYER_COUNT] = { TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE };
	};
	ui8 baseZPosition = 0;
	ui8 tileFlags = 0;
};

enum class TileShape {
    FLOOR, // Ground level, no shadow
	THIN,  // Trees
    THICK, // Solid walls
    ROOF,  // Top level flat
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

// Collision info
const float TileCollisionShapeRadii[(int)TileCollisionShape::COUNT + 1] = {
    0.0f,   // FLOOR
    0.5f,   // BOX
    0.1f,   // SMALL_CIRCLE
    0.175f, // MEDIUM_CIRCLE
	0.0f,   // COUNT (Null)
};
static_assert((int)TileCollisionShape::COUNT == 4, "Update");

struct ItemDropDef {
	nString itemName;
	ui32v2 countRange;
};
KEG_TYPE_DECL(ItemDropDef);

struct TileData {
    SpriteData spriteData;
	TileCollisionShape collisionShape = TileCollisionShape::FLOOR;
	f32 colliderHeight = 1.0f;
	f32 pathWeight = 1.0f;
    ui8v2 dims = ui8v2(1); // 4x4 is max size
	ui8 rootPos = 0;
	std::string name;
    std::string textureName;
    std::string resourceName;
	TileShape shape = TileShape::FLOOR;
	TileResource resource = TileResource::NONE;
	Array<ItemDropDef> itemDrops;
};
KEG_TYPE_DECL(TileData);

// TODO: non static
class TileRepository {
	friend class ResourceManager;
public:
	static const TileData& getTileData(TileID tileId) {
		assert(sTileData.find(tileId) != sTileData.end());
		return sTileData[tileId];
	}
	static const TileData& getTileData(const std::string& name) {
		// TOOD: Hashed string and error handling
		TileID id = sTileIdMapping[name];
		return sTileData[id];
	}
	static TileID getTile(const std::string& name) {
		return sTileIdMapping[name];
	}

private:
	static std::unordered_map<std::string, TileID> sTileIdMapping;
	static std::unordered_map<TileID, TileData> sTileData;
};

