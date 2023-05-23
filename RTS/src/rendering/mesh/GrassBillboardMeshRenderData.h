#pragma once

// Smallest data desc required to render a grass mesh
struct GrassBillboardMeshRenderData {
    VGVertexArray mVao = 0; ///< Vertex Array Object
    ui32 mIndexCount = 0;
    VGTexture mTboInstanceData = 0;
    VGTexture mTboPositionData = 0;
    VGTexture mTboNormalData = 0;
};
// Small for cache friendly drawing
static_assert(sizeof(GrassBillboardMeshRenderData) == 20);