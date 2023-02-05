#pragma once

#include "rendering/mesh/mesher/builder/MeshBuilderCommon.h"

struct MaterialData;

struct SubMeshBufferData {
    void clear() {
        mVerts.clear();
        mIndices.clear();
    }

    // TODO: Pool allocators or reserve?
    std::vector<StaticModelVertex> mVerts;
    std::vector<ui32> mIndices;
};

// Keep track of all they types of indices so we can decide to share if needed
enum class PolyTypeFlags : ui8 {
    ARRAY_TRIANGLES   = 1 << 0,
    QUADS             = 1 << 1,
    INDEXED_TRIANGLES = 1 << 2,
    COUNT = 3, // KEEP UPDATED
};

class ProceduralMeshBuilder
{
    // For access to shared IBOs
    friend class BillboardMeshBuilder;
    friend class TextMeshBuilder;
public:
    ProceduralMeshBuilder(bool useSharedIndexBuffer);
    ~ProceduralMeshBuilder();

    VORB_NON_COPYABLE_BUT_MOVABLE(ProceduralMeshBuilder);

    static void initStaticIBOs();

    void reserveVertexCount(ui32 count);
    void reserveIndexCount(ui32 count);

    void setBoundingSphere(BoundingSphere sphere) { mBoundingSphere = sphere; }

    void addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis, const MaterialData& materialData, const f32v4& uvRect, color4 color);
    void addTerrainAlignedQuad(f32v2 tilePosition, f32 terrainCorners[4], const MaterialData& materialData, color4 color, bool flipTriangleDir);
    void addTriangle(StaticModelVertex verts[3], const MaterialData& materialData, bool calculateNormals);
    void addQuadBetweenPoints(const f32v3 vertPoints[4], const MaterialData& materialData, f32v2 uvScale, color4 color, bool swapUV);
    void addQuadBetweenPoints(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const MaterialData& materialData, f32v2 uvScale, color4 color, bool swapUV);
    void addQuadBetweenPointsWorldUV(const f32v3 vertPoints[4], const MaterialData& materialData, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv = false);
    void addQuadBetweenPointsWorldUV(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const MaterialData& materialData, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv = false);
    void addBoardBetweenPoints(const f32v3& p1, const f32v3& p2, const f32v2& halfDims, const MaterialData& materialData, f32v2 uvScale);

    // Compute sphere from vertex data
    void computeBoundingSphere();

    // Upload buffers
    void finishMesh(std::unique_ptr<Mesh>& mesh, const f32v3& worldPos);
    void finishMesh(Mesh& mesh, const f32v3& worldPos);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

private:

    void uploadMeshData(MeshGpuData& subMesh, const f32v3& position, const SubMeshBufferData& data, GLbitfield flags);

    SubMeshBufferData                  mSubMeshesData[e_cast(MaterialRenderPassType::COUNT)];
    BoundingSphere                     mBoundingSphere;
    BitFlags<PolyTypeFlags>            mPolyTypeFlags;
    bool                               mUsingSharedIndexBuffer;
    bool                               mDidComputeBoundingSphere = false;

public:
    // Shared index buffers
    // TODO: UI16 Formats as well?
    static VGBuffer sQuadIboUI32;
};