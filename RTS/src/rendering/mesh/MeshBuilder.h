#pragma once

#include "Vertex.h"
#include "Mesh.h"
#include "world/TerrainConstants.h"

struct SubTexture;

typedef i32 SubmeshIndex;

// Keep track of all they types of indices so we can decide to share if needed
enum class PolyTypeFlags : ui8 {
    ARRAY_TRIANGLES   = 1 << 0,
    QUADS             = 1 << 1,
    INDEXED_TRIANGLES = 1 << 2,
    TERRAIN           = 1 << 3,
    WATER             = 1 << 4,
    COUNT = 5, // KEEP UPDATED
};

class MeshBuilder
{
    friend class BillboardMeshBuilder;
public:
    MeshBuilder(bool useSharedIndexBuffer);
    ~MeshBuilder();

    static void initStaticIBOs();

    void reserveVertexCount(ui32 count);

    void setBoundingSphere(BoundingSphere sphere) { mBoundingSphere = sphere; }

    // Geometry builders
    void setVertsTerrainFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    void setVertsWaterFromPaddedHeightfield(const f32v2& cornerPos, f32 totalWidth, const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    
    void addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis, const SubTexture& texture, const f32v4& uvRect, color4 color);
    void addTerrainAlignedQuad(f32v2 tilePosition, f32 terrainCorners[4], const SubTexture& texture, color4 color, bool flipTriangleDir);
    void addCartesianQuad(const f32v3& startPos, const f32v3& dims, CubeFacing axis, const SubTexture& texture, const f32v4& uvRect, color4 color);
    void addTriangle(StandardVertex verts[3], const SubTexture& texture, bool calculateNormals);
    void addQuadBetweenPoints(const f32v3 vertPoints[4], const SubTexture& texture, f32 uvScale, color4 color, bool isPointingUp);
    void addQuadBetweenPoints(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3, const SubTexture& texture, f32 uvScale, color4 color, bool isPointingUp);
    void addBoardBetweenPoints(const f32v3& p1, const f32v3& p2, const f32v2& halfDims, const SubTexture& texture, f32 uvScale);

    // Upload buffers
    void finishMesh(Mesh& mesh, MeshDrawMode drawMode);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

private:
    struct InProgressSubMeshData {
        void clear() {
            mVerts.clear();
            mIndices.clear();
            mTextures.clear();
        }

        // TODO: Pool allocators or reserve?
        std::vector<Vertex32> mVerts;
        std::vector<ui32> mIndices;
        std::vector<TextureHandle> mTextures;
    };

    void getSubmeshAndTextureIndex(const SubTexture& texture, OUT InProgressSubMeshData** submesh, OUT ui8* textureIndex);
    void setSharedIbo(Mesh& mesh, const bool wasUsingSharedIbo, VGBuffer sharedIbo);
    void initMeshBuffers(SubMeshData& subMesh, bool allocateUbo, bool allocateIbo);
    void uploadMeshData(SubMeshData& subMesh, const InProgressSubMeshData& data, MeshDrawMode drawMode);
    void bindVertexAttribs(SubMeshData& subMesh);

    // TODO: Try both multi-context opengl and pool_allocator
    std::unordered_map<VGTexture, std::pair<i32 /*submeshIndex*/, ui8/*textureIndex*/>> mTextureToSubmesh;
    InProgressSubMeshData              mMainSubMeshData;
    std::vector<InProgressSubMeshData> mSubMeshesData;
    BoundingSphere                     mBoundingSphere;
    BitFlags<PolyTypeFlags>            mPolyTypeFlags;
    bool                               mUsingSharedIndexBuffer;

    // Shared index buffers
    static VGBuffer sQuadIbo;
    static VGBuffer sTerrainIbo;
};