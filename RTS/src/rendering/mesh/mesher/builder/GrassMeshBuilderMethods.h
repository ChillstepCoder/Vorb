#pragma once

class Chunk;
class GrassBillboardMeshBuilder;
class HeightmapPatchData;
struct TileGrass;


namespace GrassMeshBuilderMethods
{
    void createGrassMesh(
        GrassBillboardMeshBuilder& grassMeshBuilder,
        const Chunk& chunk,
        const ui32v2& tilePosStart,
        ui32 lod,
        const HeightmapPatchData* heightData
    );

    void editorCreateGrassMesh(
        GrassBillboardMeshBuilder& grassMeshBuilder,
        ui32 widthTiles,
        const TileGrass grassDataArray[] // Should be length SQ(widthTiles)
    );
};

