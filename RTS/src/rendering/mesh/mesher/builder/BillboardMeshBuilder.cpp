#include "stdafx.h"
#include "BillboardMeshBuilder.h"

#include "math/Random.h"
#include "ProceduralMeshBuilder.h"

#include <boost/pool/singleton_pool.hpp>

struct billboard_mesh_builder_pool {};
using singleton_task_pool = boost::singleton_pool<billboard_mesh_builder_pool, sizeof(BillboardMeshBuilder), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 128u>;

constexpr ui32 MAX_SUBTEXTURES_PER_MESH = 255;

void BillboardMeshBuilder::addBillboard(const f32v3& position, const f32v2& xyDims, ui16 materialId, bool randFlip) {
    f32 xFlip;
    if (randFlip) {
        xFlip = (Random::getThreadSafef(position.x, position.y) > 0.5f) ? 1.0f : -1.0f;
    }
    else {
        xFlip = 0.0f;
    }

    mBillboards.emplace_back(BillboardData{ position, xFlip, xyDims, materialId });
}

void BillboardMeshBuilder::reserveBillboardCount(ui32 count) {
    mBillboards.reserve(count);
}

void BillboardMeshBuilder::computeBoundingSphere() {
    PROFILE_FUNCTION();

    f32v2 minMax[3] = { {FLT_MAX, -FLT_MAX}, {FLT_MAX, -FLT_MAX}, {FLT_MAX, -FLT_MAX} };

    for (auto& v : mBillboards) {
        const f32v3& pos = v.mPos;
        for (int i = 0; i < 3; ++i) {
            if (pos[i] < minMax[i].x) {
                minMax[i].x = pos[i];
            }
            if (pos[i] > minMax[i].y) {
                minMax[i].y = pos[i];
            }
        }
    }

    mBoundingSphere.center = f32v3((minMax[0].x + minMax[0].y) * 0.5f, (minMax[1].x + minMax[1].y) * 0.5f, (minMax[2].x + minMax[2].y) * 0.5f);
    const f32 halfLargestWidth = 0.5f * std::max(std::max(minMax[0].y - minMax[0].x, minMax[1].y - minMax[1].x), minMax[2].y - minMax[1].y);
    const f32 halfLargestWidthSq = SQ(halfLargestWidth);
    // TODO: This isnt quite accurate it assumes a cube instead of a rectangle, but ehh good enough for now
    mBoundingSphere.radius = sqrt(halfLargestWidthSq + halfLargestWidthSq);
}

void BillboardMeshBuilder::finishMesh(std::unique_ptr<Mesh>& mesh, const f32v3& worldPos, GLbitfield bufferFlags) {
    ASSERT_RENDER_THREAD();
    // return blank mesh if we have no geometry
    if (mBillboards.empty()) {
        mesh.reset();
        return;
    }
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }

    // Set bounds
    mesh->mPosition = worldPos;
    mesh->mBoundingSphere = mBoundingSphere;
    mesh->mBoundingSphere.center += worldPos;
    MeshBuilderCommon::initMeshBuffers(
        mesh->mGpuData,
        &ProceduralMeshBuilder::sQuadIboUI32,
        BitFlags<MeshBuilderBufferFlags>(MeshBuilderBufferFlags::NO_VBO, MeshBuilderBufferFlags::SSBO)
    );

    // TODO: Support other formats
    mesh->mGpuData.mIndexType = MeshIndexType::INT;

    // Upload data
    uploadBufferData(mesh->mGpuData, worldPos, bufferFlags);
    mBillboards.clear();
}

void BillboardMeshBuilder::uploadBufferData(MeshGpuData& subMesh, const f32v3& position, GLbitfield bufferFlags) {
    // IBO
    subMesh.mLODData.mTotalIndexCount = mBillboards.size() * 6;

    // SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, subMesh.mSSBO);
    glNamedBufferStorage(subMesh.mSSBO, sizeof(BillboardData) * mBillboards.size(), mBillboards.data(), bufferFlags);
    checkGlError("MeshBuilder::uploadMeshData");
}

void* BillboardMeshBuilder::operator new(size_t count) {
    ASSERT_GAME_THREAD();
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void BillboardMeshBuilder::operator delete(void* pointer, size_t size) {
    ASSERT_GAME_THREAD();
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}