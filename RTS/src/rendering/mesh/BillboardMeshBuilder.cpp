#include "stdafx.h"
#include "BillboardMeshBuilder.h"

#include "Random.h"
#include "MeshBuilder.h"

#include <boost/pool/singleton_pool.hpp>

struct billboard_mesh_builder_pool {};
using singleton_task_pool = boost::singleton_pool<billboard_mesh_builder_pool, sizeof(BillboardMeshBuilder), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 128u>;

#define SUBMESH_INDEX_MAIN -1
constexpr ui32 MAX_SUBTEXTURES_PER_MESH = 255;

BillboardMeshBuilder::BillboardMeshBuilder()
{

}

BillboardMeshBuilder::~BillboardMeshBuilder()
{

}

void BillboardMeshBuilder::addBillboard(f32v3 position, const f32v2& xyDims, const SubTexture& texture) {

    InProgressSubMeshData* submesh;
    ui8 subtextureIndex;
    getSubmeshAndTextureIndex(texture, &submesh, &subtextureIndex);

    f32v4 uvs = texture.mUvRect;
    if (texture.mFlags.isBitSet(SubTextureFlags::RAND_FLIP) && Random::getThreadSafef(position.x, position.y) > 0.5f) {
        // Flip horizontal
        uvs.x = uvs.x + uvs.z;
        uvs.y = uvs.y;
        uvs.z = -uvs.z;
        uvs.w = uvs.w;
    }

    submesh->mBillboards.emplace_back(BillboardData{ position, subtextureIndex, xyDims });
}

void BillboardMeshBuilder::reserveBillboardCount(ui32 count) {

    mMainSubMeshData.mBillboards.reserve(count);
    mMainSubMeshData.mSubtextureData.reserve(5); // Arbitrary
}

void BillboardMeshBuilder::finishMesh(Mesh& mesh, MeshDrawMode drawMode) {
    // return blank mesh if we have no geometry
    if (mMainSubMeshData.mBillboards.empty()) {
        return;
    }
    // Always shared
    mesh.mFlags.setBit(MeshFlags::USING_SHARED_IBO);

    // Set bounds
    mesh.mBoundingSphere = mBoundingSphere;

    // Allocate correct number of submeshes
    mesh.mSubMeshes.resize(mSubMeshesData.size());

    // Allocate all buffers if needed
    initMeshBuffers(mesh.mMainMesh);
    for (auto&& subMesh : mesh.mSubMeshes) {
        initMeshBuffers(subMesh);
    }

    // Upload data
    uploadBufferData(mesh.mMainMesh, mMainSubMeshData, drawMode);
    mMainSubMeshData.clear();
    for (size_t i = 0; i < mesh.mSubMeshes.size(); ++i) {
        uploadBufferData(mesh.mSubMeshes[i], mSubMeshesData[i], drawMode);
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

void BillboardMeshBuilder::uploadBufferData(SubMeshData& subMesh, const InProgressSubMeshData& data, MeshDrawMode drawMode)
{
    glBindVertexArray(subMesh.mVao);

    // IBO
    subMesh.mIndexCount = data.mBillboards.size() * 6;

    // UBO
    if (subMesh.mUbo) {
        const ui32 textureBufferSizeBytes = data.mSubtextureData.size() * sizeof(AlignedUboData);
        // Pack into uvec2 - https://www.khronos.org/opengl/wiki/Bindless_Texture
        AlignedUboData buffer[MAX_SUBTEXTURES_PER_MESH];
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
        glBufferData(GL_UNIFORM_BUFFER, textureBufferSizeBytes, nullptr, e_cast(drawMode));
        // Set data
        glBufferSubData(GL_UNIFORM_BUFFER, 0, textureBufferSizeBytes, buffer);
    }

    // SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, subMesh.mSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(BillboardData) * data.mBillboards.size(), data.mBillboards.data(), GL_STATIC_COPY);
    checkGlError("MeshBuilder::uploadMeshData");
}


void* BillboardMeshBuilder::operator new(size_t count) {
    assert(IS_MAIN_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void BillboardMeshBuilder::operator delete(void* pointer, size_t size) {
    assert(IS_MAIN_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}