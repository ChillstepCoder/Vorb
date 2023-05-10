#pragma once

class Chunk;
class GrassBillboardMesh;
struct HeightmapPatchData;
struct TileGrass;


namespace GrassMeshBuilder
{
    void createGrassMesh(
        GrassBillboardMesh& grassMesh,
        const Chunk& chunk,
        const ui32v2& tilePosStart,
        ui32 lod,
        const HeightmapPatchData* heightData
    );

    void editorCreateGrassMesh(
        GrassBillboardMesh& grassMesh,
        ui32 widthTiles,
        const TileGrass grassDataArray[] // Should be length SQ(widthTiles)
    );
};

