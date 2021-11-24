#pragma once

#include <Vorb/graphics/gtypes.h>
#include "MeshBase.h"

#include "rendering/TileVertex.h"
#include "rendering/RenderCommon.h"

template <typename VERTEX>
class IQuadMesh : public MeshBase {
public:
    IQuadMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(IQuadMesh);

    void setData(const VERTEX* meshData, unsigned vertexCount, MeshDrawMode drawMode);
};

class QuadMesh : public IQuadMesh<TileVertex> {
public:
    QuadMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(QuadMesh);

    void reserveQuadCount(size_t count);
    void addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, CubeFacing axis, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal);
    void addCross(f32v3 cornerPosition, ui16 spriteAtlasPage, const f32v4& uvs, float width, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence);
    void finishMesh(MeshDrawMode drawMode) override;

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<TileVertex> mVertexData; // TODO: Recycle?
};

class BillboardMesh : public IQuadMesh<BillboardVertex> {
public:
    BillboardMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(BillboardMesh);

    void reserveQuadCount(size_t count);
    void addQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence, ui8 roughness);
    void finishMesh(MeshDrawMode drawMode) override;

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<BillboardVertex> mVertexData; // TODO: Recycle?
};

// TODO: 16 bit
struct TBOBillboardInstanceData {
    f32v3 position;
    f32 type; // Lookup into uniform array
    f32 sizeX;
    f32 sizeY;

    //f32v3 rootPos;
    //i16v2 xzHalfDims;
    //ui16v4 uvRect; //TODO: ui16v2?
    //ui16 atlasPage;
    //color4 color;
    //ui8 windInfluence = 0;
    //ui8 roughness;
};
static_assert(sizeof(TBOBillboardInstanceData) == 24);
struct TBOBillboardUniformData {
    f32v4 uvRect; //TODO: ui16v2?
    f32v3 atlasPageRoughnessWind;

    bool operator==(const TBOBillboardUniformData& other) const {
        return uvRect == other.uvRect && atlasPageRoughnessWind == other.atlasPageRoughnessWind;
    }
};
class TboInstanceDataHashFunction {
public:
    size_t operator()(const TBOBillboardUniformData& d) const
    {
        return (size_t)std::hash<f32>{}(d.uvRect.x) ^
            (size_t)std::hash<f32>{}(d.uvRect.y * 127.0f) ^
            (size_t)std::hash<f32>{}(d.uvRect.z * 255.0f) ^
            (size_t)std::hash<f32>{}(d.uvRect.w * 156.0f) ^
            (size_t)std::hash<int>{}((int)d.atlasPageRoughnessWind.x * 25) ^
            (size_t)std::hash<int>{}((int)d.atlasPageRoughnessWind.y * 64) ^
            (size_t)std::hash<int>{}((int)d.atlasPageRoughnessWind.z * 525);
    }
};

class TBOBillboardMesh : public IQuadMesh<TBOBillboardInstanceData> {
public:
    TBOBillboardMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(TBOBillboardMesh);

    void beginMesh();
    void reserveQuadCount(size_t count);
    void addQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence, ui8 roughness);
    void draw(const vg::GLProgram& program) const override;
    void finishMesh(MeshDrawMode drawMode) override;
    void destroy() override;
    void setDepthSortMode(DepthSortMode mode) { mDepthSortMode = mode; }

private:

    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<TBOBillboardInstanceData> mTextureData; // TODO: Recycle?
    std::unordered_map<TBOBillboardUniformData, ui32, TboInstanceDataHashFunction> mTypes;
    ui32 mInstanceCount = 0;
    ui32 mLastTypeIndex = 0;
    VGTexture mTboTexture = 0;
    VGBuffer mUbo = 0;
    DepthSortMode mDepthSortMode = DepthSortMode::NONE;
    
};

// Templated Mesh implementation
template <typename VERTEX>
void IQuadMesh<VERTEX>::setData(const VERTEX* meshData, unsigned vertexCount, MeshDrawMode drawMode) {

    const unsigned indexCount = (vertexCount / 4) * 6;
    assert(indexCount < MAX_MESH_INDICES);

    mIndexCount = indexCount;

    const unsigned bufferSizeBytes = vertexCount * sizeof(VERTEX);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // Orphan the buffer for speed
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, enum_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, meshData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
