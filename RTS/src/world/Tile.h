#pragma once

#include "rendering/SpriteData.h"

#include <Vorb/io/Keg.h>

typedef ui16 TileID;

enum class TileLayer {
	Ground = 0,
	Mid = 1,
	Top = 2,
};

struct Tile {
	union {
		struct {
			TileID groundLayer; // Dirt, foundation, earth
			TileID midLayer; // Carpet, boards, walls, trees
			TileID topLayer; // Furniture, props
		};
		TileID layers[3];
	};
};

struct TileData {
    SpriteData spriteData;
    ui16 collisionBits = 0;
    ui8v2 dims = ui8v2(1); // 4x4 is max size
	ui8 rootPos = 0;
	std::string name;
	std::string textureName;
};
KEG_TYPE_DECL(TileData);

class TileRepository {
	friend class ResourceManager;
public:
	static TileData getTileData(TileID tileId) {
		return sTileData[tileId];
	}
	static TileData getTileData(const std::string& name) {
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

