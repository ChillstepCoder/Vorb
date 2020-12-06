#include "stdafx.h"

#include "SpriteData.h"

KEG_ENUM_DEF(TileTextureMethod, TileTextureMethod, kt) {
    kt.addValue("simple", TileTextureMethod::SIMPLE);
    kt.addValue("connected", TileTextureMethod::CONNECTED);
    kt.addValue("connected_wall", TileTextureMethod::CONNECTED_WALL);
}