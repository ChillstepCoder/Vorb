#pragma once

class Chunk;
class GrassBillboardMesh;
struct HeightmapPatchData;


namespace GrassMeshBuilder
{
    void createGrassMesh(
        GrassBillboardMesh& grassMesh,
        const Chunk& chunk,
        const ui32v2& tilePosStart,
        ui32 lod,
        const HeightmapPatchData* heightData);
};

