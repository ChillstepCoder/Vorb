#pragma once

#include <Vorb/graphics/gtypes.h>
#include "mesh/Mesh.h"

#include "rendering/TileVertex.h"
#include "rendering/RenderCommon.h"

struct GrassBillboardInstanceData {
    GrassBillboardInstanceData(ui8v2&& dims, ui8 grassType) : dims(dims), grassType(grassType) {};
    ui8v2 dims;
    ui8 grassType;
    ui8 pad;
};
static_assert(sizeof(GrassBillboardInstanceData) == 4);

class GrassBillboardMesh {
public:
    GrassBillboardMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(GrassBillboardMesh);

    void reserveQuadCount(size_t count);
    void addBladeQuad(const f32v3& tilePosition, const f32v2& xyDims, ui8 grassType);
    void draw(VGUniform tboSizeType, VGUniform tboPosition) const;
    void finishMesh(MeshDrawMode drawMode);
    void destroy();

    bool isValid() const { return mIndexCount > 0; }
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mBoundingSphere = boundingSphere; }
    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }

private:
    void initBuffers();

    BoundingSphere mBoundingSphere;  ///< Optional AABB to describe the bounds
    VGVertexArray mVao = 0; ///< Vertex Array Object
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    std::vector<GrassBillboardInstanceData> mInstanceData; // TODO: Recycle?
    std::vector<f32v3> mPositionData; // TODO: Recycle?
    VGTexture mTboInstanceData = 0;
    VGTexture mTboPositionData = 0;
    VGBuffer mVboInstanceData = 0;
    VGBuffer mVboPosition = 0;
};
