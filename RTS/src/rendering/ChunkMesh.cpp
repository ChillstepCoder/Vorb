#include "stdafx.h"
#include "ChunkMesh.h"

#include "world/Chunk.h"
#include "rendering/ChunkVertex.h"
#include "rendering/ShaderLoader.h"
#include "Camera2D.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>


vg::GLProgram ChunkMesh::sProgram;

// TODO: Data
const cString MESH_VS_SRC = R"(
uniform mat4 World;
uniform mat4 VP;

in vec4 vPosition;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;

out vec2 fUV;
flat out float fAtlasPage;
out vec4 fTint;

void main() {
    fTint = vTint;
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = World * vPosition;
    gl_Position = VP * worldPos;
}
)";
const cString MESH_FS_SRC = R"(
uniform sampler2DArray tex;

in vec2 fUV;
flat in float fAtlasPage;
in vec4 fTint;

out vec4 fColor;

void main() {
    fColor = texture(tex, vec3(fUV, fAtlasPage)) * fTint;
    // Don't write 0 alpha (TMP)
    if (fColor.a <= 0.01) {
        discard;
    }
}
)";

ChunkMesh::ChunkMesh() {
    // Create program if it's not cached
    if (!sProgram.isCreated()) sProgram = ShaderLoader::createProgram("Atlas", MESH_VS_SRC, MESH_FS_SRC);

    { // Create VAO
        glGenVertexArrays(1, &mVao);
        glBindVertexArray(mVao);

        glGenBuffers(1, &mVbo);
        // TODO: Shared IBO?
        glGenBuffers(1, &mIbo);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);

        sProgram.enableVertexAttribArrays();
        glVertexAttribPointer(sProgram.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(ChunkVertex), (void*)offsetof(ChunkVertex, pos));
        glVertexAttribPointer(sProgram.getAttribute("vUV"), 2, GL_FLOAT, false, sizeof(ChunkVertex), (void*)offsetof(ChunkVertex, uvs));
        glVertexAttribPointer(sProgram.getAttribute("vTint"), 4, GL_UNSIGNED_BYTE, true, sizeof(ChunkVertex), (void*)offsetof(ChunkVertex, color));
        glVertexAttribPointer(sProgram.getAttribute("vAtlasPage"), 1, GL_UNSIGNED_BYTE, true, sizeof(ChunkVertex), (void*)offsetof(ChunkVertex, atlasPage));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

ChunkMesh::~ChunkMesh() {
    if (mVbo != 0) {
        glDeleteBuffers(1, &mVbo);
    }
    // TODO: Shared
    if (mIbo != 0) {
        glDeleteBuffers(1, &mIbo);
    }
    if (mVao != 0) {
        glDeleteVertexArrays(1, &mVao);
    }
}

void ChunkMesh::setData(const ChunkVertex* meshData, int vertexCount, const ui32* indexData, VGTexture texture) {

    mTexture = texture;

    const unsigned indexCount = (vertexCount / 4) * 6;
    // build IBO (todo: shared)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    // Orphan the buffer for speed
    // TODO: Do we want dynamic draw or static/stream?
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(ui32), nullptr, GL_DYNAMIC_DRAW);

    mIndexCount = indexCount;
    // Set data
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mIndexCount * sizeof(ui32), indexData);

    const int bufferSizeBytes = vertexCount * sizeof(ChunkVertex);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // Orphan the buffer for speed
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_DYNAMIC_DRAW);
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, meshData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ChunkMesh::draw(const Camera2D& camera) {

    // Make sure we have been initialized
    assert(mVao);

    // Setup The Shader
    vg::DepthState::FULL.set();
    vg::RasterizerState::CULL_NONE.set();

    sProgram.use();
    f32m4 world(1.0f);
    glUniformMatrix4fv(sProgram.getUniform("World"), 1, false, &world[0][0]);
    glUniformMatrix4fv(sProgram.getUniform("VP"), 1, false, &camera.getCameraMatrix()[0][0]);

    glBindVertexArray(mVao); // TODO(Ben): This wont work with all custom shaders?

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(sProgram.getUniform("tex"), 0);

    glBindTexture(GL_TEXTURE_2D_ARRAY, mTexture);
    vg::SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D_ARRAY);

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);

    glBindVertexArray(0);

    sProgram.unuse();
}
