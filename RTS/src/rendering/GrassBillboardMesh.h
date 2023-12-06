#pragma once

#include <Vorb/graphics/gtypes.h>
#include "mesh/Mesh.h"

#include "rendering/TileVertex.h"
#include "rendering/RenderCommon.h"
#include "rendering/mesh/GrassBillboardMeshRenderData.h"

#include "rendering/mesh/TileGrassMeshType.h"

#include "resources/asset/AssetHandleBundle.h"

constexpr int GRASS_TBO_INSTANCE_DATA_BINDING = 9;
constexpr int GRASS_TBO_POSITION_DATA_BINDING = 10;
constexpr int GRASS_TBO_NORMAL_DATA_BINDING = 11;

struct GrassBillboardInstanceData {
    GrassBillboardInstanceData(ui8v2&& dims, ui8 grassType, ui8 rotation) : dims(dims), grassType(grassType), rotation(rotation) {};
    ui8v2 dims;
    ui8 grassType; // We can encode 16 possible colors and 16 possible shapes with this
    ui8 rotation; // Encodes 0 - 2PI
};
static_assert(sizeof(GrassBillboardInstanceData) == 4);

struct GrassBillboardMeshGpuData {
    GrassBillboardMeshRenderData mRenderData;
    VGBuffer mVboInstanceData = 0;
    VGBuffer mVboPosition = 0;
    VGBuffer mVboNormal = 0;

    bool isValid() const { return mRenderData.mIndexCount > 0; }
    void destroy();
};

class GrassBillboardMesh {
    friend class GrassBillboardMeshBuilder;
public:
    GrassBillboardMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(GrassBillboardMesh);

    void destroy();

    bool isValid() const { return mIsValid; }
    bool isValid(TileGrassMeshType type) const { return mData[e_cast(type)].isValid(); }
    const GrassBillboardMeshRenderData getRenderData(TileGrassMeshType type) const { return mData[e_cast(type)].mRenderData; }
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mBoundingSphere = boundingSphere; }
    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }

private:
    std::unique_ptr<AssetHandleBundle> mGrassAssets;
    BoundingSphere mBoundingSphere;  ///< Optional AABB to describe the bounds
    GrassBillboardMeshGpuData mData[e_cast(TileGrassMeshType::COUNT)];
    bool mIsValid = false;
};

class GrassMesh {
public:
    GrassMesh(ui32 patchIndex) : mIndex(patchIndex) {};

    VORB_NON_COPYABLE_BUT_MOVABLE(GrassMesh);

    GrassBillboardMesh mMesh;
    ui32 mIndex = 0;
    f32 mCrossfadeAlpha = 0.0f;
    int mCrossfadeDir = 0; // -1 = down, 0 = none, 1 = up
    f32v3 mPosition = f32v3(0.0f);
    bool mHadMesh = false;
};

class GrassBillboardMeshBuilder {
public:
    GrassBillboardMeshBuilder(GrassBillboardMesh& mesh);

    void reserveQuadCount(TileGrassMeshType type, size_t count);
    void addBladeQuad(TileGrassMeshType type, const f32v3& tilePosition, const f32v2& xyDims, ui8 grassType, ui8 rotation, const f32v3& normal);
    void finishMesh();
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mMesh.setBoundingSphere(boundingSphere); }

    const GrassBillboardMesh& getMesh() const { return mMesh; }
    void addGrassAsset(TileGrassID id) { mGrassAssets.emplace(id); }
    const std::unordered_set<TileGrassID>& getGrassAssets() const { return mGrassAssets; }
private:
    void initBuffers(int bufferIndex);

    GrassBillboardMesh& mMesh;
    std::vector<GrassBillboardInstanceData> mInstanceData[e_count(TileGrassMeshType)]; // TODO: Recycle?
    std::vector<f32v3> mPositionData[e_count(TileGrassMeshType)]; // TODO: Recycle?
    std::vector<ui8v2> mNormalData[e_count(TileGrassMeshType)]; // TODO: Recycle?
    std::unordered_set<TileGrassID> mGrassAssets;
};