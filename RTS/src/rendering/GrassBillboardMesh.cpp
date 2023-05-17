#include "stdafx.h"
#include "GrassBillboardMesh.h"

#include "math/Random.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

#include "rendering/RenderStats.h"

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

void GrassBillboardMeshBuilder::reserveQuadCount(size_t count) {
    mInstanceData.reserve(count);
    mPositionData.reserve(count);
}

void GrassBillboardMeshBuilder::addBladeQuad(const f32v3& position, const f32v2& xyDims, ui8 grassType, ui8 rotation) {
    mInstanceData.emplace_back(ui8v2(glm::min(xyDims.x, 1.0f) * 255.0f, glm::min(xyDims.y, 1.0f) * 255.0f), grassType, rotation);
    mPositionData.emplace_back(position);
}

void GrassBillboardMeshBuilder::finishMesh() {
    if (mInstanceData.size()) {
        initBuffers();

        mMesh.mIndexCount = mInstanceData.size() * 6;
        glNamedBufferStorage(mMesh.mVboPosition, sizeof(f32v3) * mPositionData.size(), mPositionData.data(), 0);
        glNamedBufferStorage(mMesh.mVboInstanceData, sizeof(GrassBillboardInstanceData) * mInstanceData.size(), mInstanceData.data(), 0);

        glTextureBuffer(mMesh.mTboInstanceData, GL_RGBA8, mMesh.mVboInstanceData);
        glTextureBuffer(mMesh.mTboPositionData, GL_RGB32F, mMesh.mVboPosition);
    }
    else {
        mMesh.destroy();
    }
    std::vector<GrassBillboardInstanceData>().swap(mInstanceData);
    std::vector<f32v3>().swap(mPositionData);
}

void GrassBillboardMeshBuilder::initBuffers() {
    if (mMesh.mVao == 0) { // Create VAO and textures
        glCreateVertexArrays(1, &mMesh.mVao);
        glVertexArrayElementBuffer(mMesh.mVao, ProceduralMeshBuilder::sQuadIboUI32);

        glCreateTextures(GL_TEXTURE_BUFFER, 1, &mMesh.mTboInstanceData);
        glCreateTextures(GL_TEXTURE_BUFFER, 1, &mMesh.mTboPositionData);
    }
    else {
        // Recreate immutable buffers
        glDeleteBuffers(1, &mMesh.mVboInstanceData);
        glDeleteBuffers(1, &mMesh.mVboPosition);
    }
    glCreateBuffers(1, &mMesh.mVboInstanceData);
    glCreateBuffers(1, &mMesh.mVboPosition);
}

void GrassBillboardMesh::draw(VGUniform tboSizeType, VGUniform tboPosition) const {
    // Make sure we have been initialized
    assert(mVao);
    if (!mIndexCount) return;

    glBindVertexArray(mVao);

    // Bind textures
    glBindTextureUnit(10, mTboInstanceData);
    glBindTextureUnit(11, mTboPositionData);
    glUniform1i(tboSizeType, 10);
    glUniform1i(tboPosition, 11);

    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glDrawElements(GL_PATCHES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3);
}

void GrassBillboardMesh::destroy()
{
    if (mVao != 0) {
        GL.glDeleteBuffers(1, &mVboInstanceData);
        mVboInstanceData = 0;
        GL.glDeleteBuffers(1, &mVboPosition);
        mVboPosition = 0;
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
        glDeleteTextures(1, &mTboInstanceData);
        glDeleteTextures(1, &mTboPositionData);
    }

    mIndexCount = 0;
}
