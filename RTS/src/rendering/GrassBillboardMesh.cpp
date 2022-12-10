#include "stdafx.h"
#include "GrassBillboardMesh.h"

#include "math/Random.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

#include "rendering/RenderStats.h"

#include "mesh/ProceduralMeshBuilder.h"

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
static constexpr float EPSILON = 0.005f;

void GrassBillboardMesh::reserveQuadCount(size_t count) {
    mInstanceData.reserve(count);
    mPositionData.reserve(count);
}

void GrassBillboardMesh::addBladeQuad(const f32v3& position, const f32v2& xyDims, ui8 grassType) {
    mInstanceData.emplace_back(ui8v2(glm::min(xyDims.x, 1.0f) * 255.0f, glm::min(xyDims.y, 1.0f) * 255.0f), grassType);
    mPositionData.emplace_back(position);
}

void GrassBillboardMesh::draw(VGUniform tboSizeType, VGUniform tboPosition) const
{
    // Make sure we have been initialized
    assert(mVao);
    if (!mIndexCount) return;

    glBindVertexArray(mVao);

    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_BUFFER, mTboInstanceData);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_BUFFER, mTboPositionData);

    // Bind uniforms
    glUniform1i(tboSizeType, 10);
    glUniform1i(tboPosition, 11);

    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glDrawElements(GL_PATCHES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3);
    glBindVertexArray(0);
}

void GrassBillboardMesh::finishMesh(MeshDrawMode drawMode)
{
    if (mInstanceData.size()) {
        initBuffers();

        mIndexCount = mInstanceData.size() * 6;

        glBindBuffer(GL_TEXTURE_BUFFER, mVboPosition);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(f32v3) * mPositionData.size(), nullptr, (GLenum)drawMode);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(f32v3) * mPositionData.size(), mPositionData.data());

        glBindBuffer(GL_TEXTURE_BUFFER, mVboInstanceData);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(GrassBillboardInstanceData) * mInstanceData.size(), nullptr, (GLenum)drawMode);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(GrassBillboardInstanceData) * mInstanceData.size(), mInstanceData.data());

        glBindTexture(GL_TEXTURE_BUFFER, mTboInstanceData);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, mVboInstanceData);
        glBindTexture(GL_TEXTURE_BUFFER, mTboPositionData);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, mVboPosition);

        glBindVertexArray(0);
    }
    else {
        destroy();
    }
    std::vector<GrassBillboardInstanceData>().swap(mInstanceData);
    std::vector<f32v3>().swap(mPositionData);
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

void GrassBillboardMesh::initBuffers() {
    if (mVao == 0) { // Create VAO
        glGenVertexArrays(1, &mVao);
        glBindVertexArray(mVao);

        glGenBuffers(1, &mVboInstanceData);
        glGenBuffers(1, &mVboPosition);

        glBindBuffer(GL_ARRAY_BUFFER, 0); // Hack, no data at all, the shader generates vertex positions
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ProceduralMeshBuilder::sQuadIbo);

        glGenTextures(1, &mTboInstanceData);
        glGenTextures(1, &mTboPositionData);
    }
    else {
        glBindVertexArray(mVao);
    }
}