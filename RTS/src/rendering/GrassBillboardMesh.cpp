#include "stdafx.h"
#include "GrassBillboardMesh.h"

#include "math/Random.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

#include "mesh/mesher/builder/ProceduralMeshBuilder.h"

#include "rendering/gl/GL.h"

// Must match glsl
constexpr ui32 MAX_UNIFORM_ARRAY_SIZE = 256; // TODO: Query hardware + defines? Need to assert if uniform buffer size < 16kb

const f32v2 CUBE_FACING_AXIS_DIRECTIONS[e_cast(CubeFacing::COUNT)] = {
    f32v2(-1, 1), // LEFT
    f32v2(1,  1),  // FRONT
    f32v2(1,  1),  // RIGHT
    f32v2(-1, 1), // BACK
    f32v2(1,  1),  // TOP
    f32v2(-1, -1)   // BOTTOM
};
const f32v2 CUBE_FACING_AXIS_INITIAL_OFFSETS[e_cast(CubeFacing::COUNT)] = {
    f32v2(1, 0), // LEFT
    f32v2(0, 0),  // FRONT
    f32v2(0, 0),  // RIGHT
    f32v2(1, 0), // BACK
    f32v2(0, 0),  // TOP
    f32v2(1, 1)   // BOTTOM
};

// Define all possible templates for Mesh class
// Each function definition should be proceeded by this
// 
// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;

GrassBillboardMeshBuilder::GrassBillboardMeshBuilder(GrassBillboardMesh& mesh) : mMesh(mesh) {

}

void GrassBillboardMeshBuilder::reserveQuadCount(TileGrassMeshType type, size_t count) {
    mInstanceData[e_cast(type)].reserve(count);
    mPositionData[e_cast(type)].reserve(count);
    mNormalData[e_cast(type)].reserve(count);
}

void GrassBillboardMeshBuilder::addBladeQuad(TileGrassMeshType type, const f32v3& position, const f32v2& xyDims, ui8 grassType, ui8 rotation, const f32v3& normal) {
    mInstanceData[e_cast(type)].emplace_back(ui8v2(glm::min(xyDims.x, 1.0f) * 255.0f, glm::min(xyDims.y, 1.0f) * 255.0f), grassType, rotation);
    mPositionData[e_cast(type)].emplace_back(position);
    mNormalData[e_cast(type)].emplace_back(ui8v2((ui8)round((normal.x + 1.0f) * 127.5f), (ui8)round((normal.y + 1.0) * 127.5f)));
}

void GrassBillboardMeshBuilder::finishMesh() {
    mMesh.mIsValid = false; // Mesh is valid if it has any renderable data
    for (int i = 0; i < e_count(TileGrassMeshType); ++i) {
        if (mInstanceData[i].size()) {
            GrassBillboardMeshGpuData& gpuData = mMesh.mData[i];
            GrassBillboardMeshRenderData& renderData = gpuData.mRenderData;
            initBuffers(i);

            renderData.mIndexCount = mInstanceData[i].size() * 6;
            glNamedBufferStorage(gpuData.mVboInstanceData, sizeof(GrassBillboardInstanceData) * mInstanceData[i].size(), mInstanceData[i].data(), 0);
            glNamedBufferStorage(gpuData.mVboPosition, sizeof(f32v3) * mPositionData[i].size(), mPositionData[i].data(), 0);
            glNamedBufferStorage(gpuData.mVboNormal, sizeof(i8v2) * mNormalData[i].size(), mNormalData[i].data(), 0);

            glTextureBuffer(renderData.mTboInstanceData, GL_RGBA8, gpuData.mVboInstanceData);
            glTextureBuffer(renderData.mTboPositionData, GL_RGB32F, gpuData.mVboPosition);
            glTextureBuffer(renderData.mTboNormalData, GL_RG8, gpuData.mVboNormal);

            mMesh.mIsValid = true;
        }
        else {
            mMesh.mData[i].destroy();
        }
        std::vector<GrassBillboardInstanceData>().swap(mInstanceData[i]);
        std::vector<f32v3>().swap(mPositionData[i]);
        std::vector<ui8v2>().swap(mNormalData[i]);
    }
}

void GrassBillboardMeshBuilder::initBuffers(int bufferIndex) {
    GrassBillboardMeshGpuData& gpuData = mMesh.mData[bufferIndex];
    GrassBillboardMeshRenderData& renderData = gpuData.mRenderData;
    if (renderData.mVao == 0) { // Create VAO and textures
        glCreateVertexArrays(1, &renderData.mVao);
        glVertexArrayElementBuffer(renderData.mVao, ProceduralMeshBuilder::sQuadIboUI32);

        glCreateTextures(GL_TEXTURE_BUFFER, 1, &renderData.mTboInstanceData);
        glCreateTextures(GL_TEXTURE_BUFFER, 1, &renderData.mTboPositionData);
        glCreateTextures(GL_TEXTURE_BUFFER, 1, &renderData.mTboNormalData);
    }
    else {
        // Recreate immutable buffers
        glDeleteBuffers(1, &gpuData.mVboInstanceData);
        glDeleteBuffers(1, &gpuData.mVboPosition);
        glDeleteBuffers(1, &gpuData.mVboNormal);
    }
    glCreateBuffers(1, &gpuData.mVboInstanceData);
    glCreateBuffers(1, &gpuData.mVboPosition);
    glCreateBuffers(1, &gpuData.mVboNormal);
}

void GrassBillboardMesh::destroy()
{
    for (int i = 0; i < e_count(TileGrassMeshType); ++i) {
        mData[i].destroy();
    }
}

void GrassBillboardMeshGpuData::destroy() {
    if (mRenderData.mVao != 0) {
        GL.glDeleteBuffers(1, &mVboInstanceData);
        mVboInstanceData = 0;
        GL.glDeleteBuffers(1, &mVboPosition);
        mVboPosition = 0;
        GL.glDeleteBuffers(1, &mVboNormal);
        mVboNormal = 0;
        glDeleteVertexArrays(1, &mRenderData.mVao);
        mRenderData.mVao = 0;
        glDeleteTextures(1, &mRenderData.mTboInstanceData);
        glDeleteTextures(1, &mRenderData.mTboPositionData);
        glDeleteTextures(1, &mRenderData.mTboNormalData);
        mRenderData.mIndexCount = 0;
    }
}
