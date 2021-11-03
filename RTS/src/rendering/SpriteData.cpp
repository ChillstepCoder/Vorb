#include "stdafx.h"

#include "SpriteData.h"

KEG_ENUM_DEF(TileTextureMethod, TileTextureMethod, kt) {
    kt.addValue("simple", TileTextureMethod::SIMPLE);
    kt.addValue("connected", TileTextureMethod::CONNECTED);
    kt.addValue("connected_wall", TileTextureMethod::CONNECTED_WALL);
    kt.addValue("vertical", TileTextureMethod::VERTICAL);
    kt.addValue("flora", TileTextureMethod::FLORA);
    kt.addValue("world_tiling", TileTextureMethod::WORLD_TILING);
}
static_assert(enum_cast(TileTextureMethod::COUNT) == 6);