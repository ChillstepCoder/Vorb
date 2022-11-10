#pragma once

#include "rendering/mesh/MeshBuilderCommon.h"
#include "world/TerrainConstants.h"

struct SubTexture;


// Keep track of all they types of indices so we can decide to share if needed
enum class PolyTypeFlags : ui8 {
    ARRAY_TRIANGLES   = 1 << 0,
    QUADS             = 1 << 1,
    INDEXED_TRIANGLES = 1 << 2,
    TERRAIN           = 1 << 3,
    WATER             = 1 << 4,
    COUNT = 5, // KEEP UPDATED
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

    // Geometry builders
    void setVertsTerrainFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    void setVertsWaterFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    
    void addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis, const SubTexture& texture, const f32v4& uvRect, color4 color);
    void addTerrainAlignedQuad(f32v2 tilePosition, f32 terrainCorners[4], const SubTexture& texture, color4 color, bool flipTriangleDir);
    void addTriangle(StandardVertex verts[3], const SubTexture& texture, bool calculateNormals);
    void addQuadBetweenPoints(const f32v3 vertPoints[4], const SubTexture& texture, f32v2 uvScale, color4 color);
    void addQuadBetweenPoints(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const SubTexture& texture, f32v2 uvScale, color4 color);
    void addQuadBetweenPointsWorldUV(const f32v3 vertPoints[4], const SubTexture& texture, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv = false);
    void addQuadBetweenPointsWorldUV(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const SubTexture& texture, f32v2 uvScale, color4 color, AXIS_3D uvOrient, const f32v3& worldUVRoot, bool flipUv = false);
    void addBoardBetweenPoints(const f32v3& p1, const f32v3& p2, const f32v2& halfDims, const SubTexture& texture, f32v2 uvScale);

    // Compute sphere from vertex data
    void computeBoundingSphere();

    // Upload buffers
    void finishMesh(std::unique_ptr<Mesh>& mesh, MeshDrawMode drawMode, const f32v3& worldPos);
    void finishMesh(Mesh& mesh, MeshDrawMode drawMode, const f32v3& worldPos);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

private:

    void getSubmeshAndTextureIndex(const SubTexture& texture, OUT SubMeshBufferData** submesh, OUT ui8* textureIndex);
    void uploadMeshData(SubMeshData& subMesh, const f32v3& position, const SubMeshBufferData& data, MeshDrawMode drawMode);
    void bindVertexAttribs(SubMeshData& subMesh);

    // TODO: Try both multi-context opengl and pool_allocator
    std::unordered_map<VGTexture, std::pair<i32 /*submeshIndex*/, ui8/*textureIndex*/>> mTextureToSubmesh;
    std::vector<SubMeshBufferData> mSubMeshesData;
    BoundingSphere                     mBoundingSphere;
    BitFlags<PolyTypeFlags>            mPolyTypeFlags;
    bool                               mUsingSharedIndexBuffer;
    bool                               mDidComputeBoundingSphere = false;

public:
    // Shared index buffers
    static VGBuffer sQuadIbo;
    static VGBuffer sTerrainIbo;
};