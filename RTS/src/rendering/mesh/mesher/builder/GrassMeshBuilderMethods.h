#pragma once

class Chunk;
class GrassBillboardMeshBuilder;
class HeightmapPatch;
struct TileGrass;


namespace GrassMeshBuilderMethods
{
    // Return false on failure
    bool createGrassMesh(
        GrassBillboardMeshBuilder& grassMeshBuilder,
        const Chunk& chunk,
        const ui32v2& tilePosStart,
        ui32 lod
    );

    void editorCreateGrassMesh(
        GrassBillboardMeshBuilder& grassMeshBuilder,
        ui32 widthTiles,
        const TileGrass grassDataArray[] // Should be length SQ(widthTiles)
    );
};

