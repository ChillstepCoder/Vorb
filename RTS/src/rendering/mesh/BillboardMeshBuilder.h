#pragma once

#include "Mesh.h"
#include "rendering/texture/SubTexture.h"

#include <boost/container_hash/hash.hpp>

// TBO Billboards
struct SubtextureUniformData {
    f32v4 uvRect; //TODO: ui16v2?
    TextureHandle textureDiffuse;
    TextureHandle textureNormal;
     // TODO: Color?
};

class BillboardMeshBuilder
{
public:
    BillboardMeshBuilder();
    ~BillboardMeshBuilder();
    VORB_NON_COPYABLE_BUT_MOVABLE(BillboardMeshBuilder);

    void addBillboard(f32v3 position, const f32v2& xyDims, const SubTexture& texture);
    void reserveBillboardCount(ui32 count);

    void computeBoundingSphere();
    void finishMesh(std::unique_ptr<Mesh>& mesh, MeshDrawMode drawMode, const f32v3& worldPos);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);
private:

    struct BillboardData {
        f32v3 mPos;
        int mTexture;
        f32v2 mDims;
        f32 mXFlip;
        f32 PADDING;
    };
    static_assert(sizeof(BillboardData) == 32);

    struct InProgressSubMeshData {
        void clear() {
            mBillboards.clear();
            mSubtextureData.clear();
        }

        // TODO: Reserve? Pool allocators?
        std::vector<BillboardData> mBillboards;
        std::vector<SubtextureUniformData> mSubtextureData;
    };

    void getSubmeshAndTextureIndex(const SubTexture& texture, OUT InProgressSubMeshData** submesh, OUT ui8* subtextureIndex);
    void initMeshBuffers(SubMeshData& subMesh);
    void uploadBufferData(SubMeshData& subMesh, const f32v3& position, const InProgressSubMeshData& data, MeshDrawMode drawMode);

    // Map subtexture IDs to submeshes 
    std::unordered_map<SubTextureID, std::pair<i32 /*submeshIndex*/, ui8/*textureIndex*/> > mSubtextureLookup;
    std::vector<InProgressSubMeshData> mSubMeshesData;
    BoundingSphere                     mBoundingSphere;
};

