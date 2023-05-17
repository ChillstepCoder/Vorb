#pragma once

#include <Vorb/graphics/gtypes.h>
#include "mesh/Mesh.h"

#include "rendering/TileVertex.h"
#include "rendering/RenderCommon.h"

#include "rendering/mesh/TileGrassMeshType.h"

struct GrassBillboardInstanceData {
    GrassBillboardInstanceData(ui8v2&& dims, ui8 grassType, ui8 rotation) : dims(dims), grassType(grassType), rotation(rotation) {};
    ui8v2 dims;
    ui8 grassType; // We can encode 16 possible colors and 16 possible shapes with this
    ui8 rotation; // Encodes 0 - 2PI
};
static_assert(sizeof(GrassBillboardInstanceData) == 4);

class GrassBillboardMesh {
    friend class GrassBillboardMeshBuilder;
public:
    GrassBillboardMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(GrassBillboardMesh);

    void draw(VGUniform tboSizeType, VGUniform tboPosition) const;
    void destroy();

    bool isValid() const { return mIndexCount > 0; }
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mBoundingSphere = boundingSphere; }
    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }

private:
    BoundingSphere mBoundingSphere;  ///< Optional AABB to describe the bounds
    VGVertexArray mVao = 0; ///< Vertex Array Object
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    VGTexture mTboInstanceData = 0;
    VGTexture mTboPositionData = 0;
    VGBuffer mVboInstanceData = 0;
    VGBuffer mVboPosition = 0;
};

class GrassMesh {
public:
    GrassMesh(ui32 patchIndex) : mIndex(patchIndex) {};

    VORB_NON_COPYABLE_BUT_MOVABLE(GrassMesh);

    GrassBillboardMesh mMesh;
    ui32 mIndex = 0;
    std::atomic<f32> mCrossfadeAlpha = 0.0f;
    std::atomic_int mCrossfadeDir = 0; // -1 = down, 0 = none, 1 = up
    f32v3 mPosition = f32v3(0.0f);
    bool mHadMesh = false;
};

class GrassBillboardMeshBuilder {
public:
    GrassBillboardMeshBuilder(GrassBillboardMesh& mesh);

    void reserveQuadCount(size_t count);
    void addBladeQuad(const f32v3& tilePosition, const f32v2& xyDims, ui8 grassType, ui8 rotation);
    void finishMesh();
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mMesh.setBoundingSphere(boundingSphere); }

    const GrassBillboardMesh& getMesh() const { return mMesh; }
private:
    void initBuffers();

    GrassBillboardMesh& mMesh;
    std::vector<GrassBillboardInstanceData> mInstanceData; // TODO: Recycle?
    std::vector<f32v3> mPositionData; // TODO: Recycle?
};