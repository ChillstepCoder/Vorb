#pragma once

#include "rendering/SpriteData.h"

constexpr ui16 TILE_ID_NONE = UINT16_MAX;
typedef ui16 TileID;
constexpr int TILE_LAYER_COUNT = 3;

enum class TileLayer {
	Ground = 0,
	Mid = 1,
	Top = 2,
};

struct Tile {
	Tile() {};
    Tile(TileID ground, TileID mid, TileID top) : groundLayer(ground), midLayer(mid), topLayer(top) { }
    Tile(TileID ground, TileID mid, TileID top, ui16 zPos) : groundLayer(ground), midLayer(mid), topLayer(top), baseZPosition(zPos){ }

	union {
		struct {
			TileID groundLayer; // Dirt, foundation, earth
			TileID midLayer; // Carpet, boards, walls, trees
			TileID topLayer; // Furniture, props
		};
		TileID layers[TILE_LAYER_COUNT] = { TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE };
	};
	ui16 baseZPosition = 0;
};

enum class TileShape {
    FLOOR, // Ground level, no shadow
	THIN,  // Trees
    THICK, // Solid walls
    ROOF,  // Top level flat
	COUNT
};
KEG_ENUM_DECL(TileShape);

enum class TileCollisionShape {
	FLOOR,
	BOX,
	SMALL_CIRCLE,
	MEDIUM_CIRCLE,
	COUNT
};

struct TileData {
    SpriteData spriteData;
	TileCollisionShape collisionShape = TileCollisionShape::FLOOR;
	f32 colliderHeight = 1.0f;
    ui8v2 dims = ui8v2(1); // 4x4 is max size
	ui8 rootPos = 0;
	std::string name;
	std::string textureName;
	TileShape shape = TileShape::FLOOR;
};
KEG_TYPE_DECL(TileData);

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

