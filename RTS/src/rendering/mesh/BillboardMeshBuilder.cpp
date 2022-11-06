#include "stdafx.h"
#include "BillboardMeshBuilder.h"

#include "math/Random.h"
#include "MeshBuilder.h"

#include <boost/pool/singleton_pool.hpp>

struct billboard_mesh_builder_pool {};
using singleton_task_pool = boost::singleton_pool<billboard_mesh_builder_pool, sizeof(BillboardMeshBuilder), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 128u>;

#define SUBMESH_INDEX_MAIN -1
constexpr ui32 MAX_SUBTEXTURES_PER_MESH = 255;

BillboardMeshBuilder::BillboardMeshBuilder() {

}

BillboardMeshBuilder::~BillboardMeshBuilder() {

}

void BillboardMeshBuilder::addBillboard(f32v3 position, const f32v2& xyDims, const SubTexture& texture) {

    InProgressSubMeshData* submesh;
    ui8 subtextureIndex;
    getSubmeshAndTextureIndex(texture, &submesh, &subtextureIndex);

    const f32 xFlip = (f32)(texture.mFlags.isBitSet(SubTextureFlags::RAND_FLIP) && Random::getThreadSafef(position.x, position.y) > 0.5f) ? 1.0f : -1.0f;

    submesh->mBillboards.emplace_back(BillboardData{ position, subtextureIndex, xyDims, xFlip });
}

void BillboardMeshBuilder::reserveBillboardCount(ui32 count) {

    mMainSubMeshData.mBillboards.reserve(count);
    mMainSubMeshData.mSubtextureData.reserve(5); // Arbitrary
}

void BillboardMeshBuilder::computeBoundingSphere() {
    PROFILE_FUNCTION();

    f32v2 minMax[3] = { {FLT_MAX, FLT_MIN}, {FLT_MAX, FLT_MIN}, {FLT_MAX, FLT_MIN} };

    for (auto& v : mMainSubMeshData.mBillboards) {
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
    for (auto& subMesh : mSubMeshesData) {
        for (auto& v : subMesh.mBillboards) {
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
    }
    mBoundingSphere.center = f32v3((minMax[0].x + minMax[0].y) * 0.5f, (minMax[1].x + minMax[1].y) * 0.5f, (minMax[2].x + minMax[2].y) * 0.5f);
    const f32 halfLargestWidth = 0.5f * std::max(std::max(minMax[0].y - minMax[0].x, minMax[1].y - minMax[1].x), minMax[2].y - minMax[1].y);
    const f32 halfLargestWidthSq = SQ(halfLargestWidth);
    // TODO: This isnt quite accurate it assumes a cube instead of a rectangle, but ehh good enough for now
    mBoundingSphere.radius = sqrt(halfLargestWidthSq + halfLargestWidthSq);
}

void BillboardMeshBuilder::finishMesh(std::unique_ptr<Mesh>& mesh, MeshDrawMode drawMode, const f32v3& worldPos) {
    assert(IS_RENDER_THREAD());
    // return blank mesh if we have no geometry
    if (mMainSubMeshData.mBillboards.empty()) {
        mesh.reset();
        return;
    }
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }

    // Always shared
    mesh->mFlags.setBit(MeshFlags::USING_SHARED_IBO);

    // Set bounds
    mesh->mPosition = worldPos;
    mesh->mBoundingSphere = mBoundingSphere;
    mesh->mBoundingSphere.center += worldPos;

    // Allocate correct number of submeshes
    mesh->mSubMeshes.resize(mSubMeshesData.size());

    // Allocate all buffers if needed
    initMeshBuffers(mesh->mMainMesh);
    for (auto&& subMesh : mesh->mSubMeshes) {
        initMeshBuffers(subMesh);
    }

    // Upload data
    uploadBufferData(mesh->mMainMesh, worldPos, mMainSubMeshData, drawMode);
    mMainSubMeshData.clear();
    for (size_t i = 0; i < mesh->mSubMeshes.size(); ++i) {
        uploadBufferData(mesh->mSubMeshes[i], worldPos, mSubMeshesData[i], drawMode);
        mSubMeshesData[i].clear();
    }

    // Cleanup
    // TODO: Do we need this really?
    mSubMeshesData.clear();
    mSubtextureLookup.clear();

    glBindVertexArray(0);
}

void BillboardMeshBuilder::getSubmeshAndTextureIndex(const SubTexture& texture, OUT InProgressSubMeshData** submesh, OUT ui8* subtextureIndex) {
    auto&& it = mSubtextureLookup.find(texture.mId);
    if (it != mSubtextureLookup.end()) {
        i32 submeshIndex = it->second.first;
        *subtextureIndex = it->second.second;
        if (submeshIndex == SUBMESH_INDEX_MAIN) {
            *submesh = &mMainSubMeshData;
        }
        else {
            *submesh = &mSubMeshesData[submeshIndex];
        }
    }
    else {
        if (mMainSubMeshData.mSubtextureData.size() < MAX_SUBTEXTURES_PER_MESH) {
            // This texture fits in the main submesh
            *subtextureIndex = mMainSubMeshData.mSubtextureData.size();
            mMainSubMeshData.mSubtextureData.emplace_back(SubtextureUniformData{ texture.mUvRect, texture.mTextureHandleDiffuse, texture.mTextureHandleNormal });
            mSubtextureLookup[texture.mId] = std::make_pair(SUBMESH_INDEX_MAIN, *subtextureIndex);
            *submesh = &mMainSubMeshData;
        }
        else {
            // Our main mesh has too many textures already, find a valid submesh for it
            bool foundSubmesh = false;
            for (size_t i = 0; i < mSubMeshesData.size(); ++i) {
                InProgressSubMeshData& data = mSubMeshesData[i];
                if (data.mSubtextureData.size() < MAX_SUBTEXTURES_PER_MESH) {
                    // This texture fits in the main submesh
                    *subtextureIndex = data.mSubtextureData.size();
                    mMainSubMeshData.mSubtextureData.emplace_back(SubtextureUniformData{ texture.mUvRect, texture.mTextureHandleDiffuse, texture.mTextureHandleNormal });
                    mSubtextureLookup[texture.mId] = std::make_pair(i, *subtextureIndex);
                    foundSubmesh = true;
                    *submesh = &data;
                    break;
                }
            }
            // No valid submesh, make a new submesh
            if (!foundSubmesh) {
                InProgressSubMeshData& data = mSubMeshesData.emplace_back();
                *subtextureIndex = 0;
                mMainSubMeshData.mSubtextureData.emplace_back(SubtextureUniformData{ texture.mUvRect, texture.mTextureHandleDiffuse, texture.mTextureHandleNormal });
                mSubtextureLookup[texture.mId] = std::make_pair(mSubMeshesData.size() - 1, *subtextureIndex);
                *submesh = &data;
            }
        }
    }
}

void BillboardMeshBuilder::initMeshBuffers(SubMeshData& subMesh)
{
    // VAO
    if (subMesh.mVao == 0) {
        glGenVertexArrays(1, &subMesh.mVao);
    }
    glBindVertexArray(subMesh.mVao);    
    // UBO
    if (subMesh.mUbo == 0) {
        glGenBuffers(1, &subMesh.mUbo);
        glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, subMesh.mUbo);
    }
    // SSBO
    if (subMesh.mSSBO == 0) {
        glGenBuffers(1, &subMesh.mSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, subMesh.mSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, subMesh.mSSBO);
    }
    // IBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, MeshBuilder::sQuadIbo);

    checkGlError("BillboardMeshBuilder::initMeshBuffers");
}

// TODO: Just build this natively? Why do a copy?
struct AlignedUboData {
    f32v4 uvs;
    ui32v4 textures;    
};

void BillboardMeshBuilder::uploadBufferData(SubMeshData& subMesh, const f32v3& position, const InProgressSubMeshData& data, MeshDrawMode drawMode)
{
    glBindVertexArray(subMesh.mVao);

    // IBO
    subMesh.mIndexCount = data.mBillboards.size() * 6;

    // UBO
    assert(subMesh.mUbo);
    const ui32 uboSizeBytes = sizeof(f32v4) + data.mSubtextureData.size() * sizeof(AlignedUboData);
    // Pack into uvec2 - https://www.khronos.org/opengl/wiki/Bindless_Texture
    // Front of the array is a vec4 (vec3 position)
    constexpr size_t BUFFER_SIZE = sizeof(f32v4) + MAX_SUBTEXTURES_PER_MESH * sizeof(AlignedUboData);
    ui8 byteBuffer[BUFFER_SIZE];
    // Set position
    *(f32v3*)byteBuffer = position;
    AlignedUboData* buffer = (AlignedUboData*)(byteBuffer + sizeof(f32v4));
    for (ui32 i = 0; i < data.mSubtextureData.size(); ++i) {
        const SubtextureUniformData& subtextureData = data.mSubtextureData[i];
        buffer[i].uvs = subtextureData.uvRect;
        buffer[i].textures.x = subtextureData.textureDiffuse & 0xffffffff;
        buffer[i].textures.y = subtextureData.textureDiffuse >> 32;
        buffer[i].textures.z = subtextureData.textureNormal & 0xffffffff;
        buffer[i].textures.w = subtextureData.textureNormal >> 32;
    }
    // Allocate orphaned
    glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
    glBufferData(GL_UNIFORM_BUFFER, uboSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_UNIFORM_BUFFER, 0, uboSizeBytes, byteBuffer);

    // SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, subMesh.mSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(BillboardData) * data.mBillboards.size(), data.mBillboards.data(), GL_STATIC_COPY);
    checkGlError("MeshBuilder::uploadMeshData");
}


void* BillboardMeshBuilder::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void BillboardMeshBuilder::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}