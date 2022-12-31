#pragma once

#include "Mesh.h"
#include "rendering/texture/SubTexture.h"

#include <boost/container_hash/hash.hpp>

class BillboardMeshBuilder
{
public:
    BillboardMeshBuilder() = default;
    ~BillboardMeshBuilder() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(BillboardMeshBuilder);

    void addBillboard(const f32v3& position, const f32v2& xyDims, ui16 materialId, bool randFlip);
    void reserveBillboardCount(ui32 count);

    void computeBoundingSphere();
    void finishMesh(std::unique_ptr<Mesh>& mesh, const f32v3& worldPos, GLbitfield bufferFlags);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);
private:

    // TODO: This doesnt need to be 32! We can compress it! (Actually due to std430 it is, unless we make them array of scalars?)
    struct PACKED_STRUCT BillboardData {
        f32v3 mPos;
        f32 mXFlip; // TODO: ui8?
        f32v2 mDims;
        ui32 mMaterialId;
        f32 PADDING;
    };
    static_assert(sizeof(BillboardData) == 32);

    void uploadBufferData(MeshGpuData& subMesh, const f32v3& position, GLbitfield bufferFlags);

    std::vector<BillboardData>         mBillboards;
    BoundingSphere                     mBoundingSphere;
};

