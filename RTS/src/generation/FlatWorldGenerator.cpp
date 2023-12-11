#include "stdafx.h"
#include "FlatWorldGenerator.h"

#include "tile/Tile.h"
#include "tile/TileGrass.h"

constexpr f32 BASE_HEIGHT = 10.0f;

Tile FlatWorldGenerator::generateTileAtPos(const f32v2& worldPos, f32 height, f32v3 normal, TileGrass* grass, const BiomeDef* biome) {

    static TileGrassID defaultGrass = 0; // TODO: DIFFERENT
    constexpr f32 GRASS_SCALE = 2.0f;
    constexpr f32 GRASS_OFFSET = 0.45f;
    const f32 grassNoise = mGenerationData.mGrassNoise.compute(worldPos.x, worldPos.y);
    const ui8 density = (ui8)glm::clamp(glm::round((grassNoise * GRASS_SCALE + GRASS_OFFSET) * 255.0f), 0.0f, 255.0f);
    grass->grassIDs[0] = defaultGrass;
    grass->densities[0] = density;

    Tile tile;
    tile.groundZOffset = height;
    return tile;
}
