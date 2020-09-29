#include "stdafx.h"
#include "QuadMesh.h"

#include "world/Chunk.h"
#include "rendering/BasicVertex.h"
#include "Camera2D.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

constexpr unsigned MAX_INDICES = CHUNK_SIZE * 4 * 6;

bool sQuadIndicesInitialized = false;
ui32 sQuadIndices[MAX_INDICES];

void initSharedQuadIndices() {
    ui32 i = 0;
    for (ui32 v = 0; i < MAX_INDICES; v += 4u) {
        sQuadIndices[i++] = v;
        sQuadIndices[i++] = v + 2;
        sQuadIndices[i++] = v + 3;
        sQuadIndices[i++] = v + 3;
        sQuadIndices[i++] = v + 1;
        sQuadIndices[i++] = v;
    }
    sQuadIndicesInitialized = true;
}

QuadMesh::QuadMesh() {

    { // Create VAO
        glGenVertexArrays(1, &mVao);
        glBindVertexArray(mVao);

        glGenBuffers(1, &mVbo);
        // TODO: Shared IBO?
        glGenBuffers(1, &mIbo);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

QuadMesh::~QuadMesh() {
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

void QuadMesh::setData(const BasicVertex* meshData, int vertexCount, VGTexture texture) {

    if (!sQuadIndicesInitialized) {
        initSharedQuadIndices();
    }

    mTexture = texture;

    const unsigned indexCount = (vertexCount / 4) * 6;
    assert(indexCount < MAX_INDICES);

    // build IBO (todo: shared)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    // Orphan the buffer for speed
    // TODO: Do we want dynamic draw or static/stream?
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(ui32), nullptr, GL_DYNAMIC_DRAW);

    mIndexCount = indexCount;
    // TODO: Can we get away with just a single IBO?
    // Set data
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mIndexCount * sizeof(ui32), sQuadIndices);

    const int bufferSizeBytes = vertexCount * sizeof(BasicVertex);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // Orphan the buffer for speed
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_DYNAMIC_DRAW);
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, meshData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void QuadMesh::draw(const Camera2D& camera, vg::GLProgram& program) {

    // Make sure we have been initialized
    assert(mVao);

    // Setup The Shader
    vg::DepthState::FULL.set();
    vg::RasterizerState::CULL_NONE.set();

    program.use();
    f32m4 world(1.0f);
    // TODO: cache uniform id?
    glUniformMatrix4fv(program.getUniform("World"), 1, false, &world[0][0]);
    glUniformMatrix4fv(program.getUniform("VP"), 1, false, &camera.getCameraMatrix()[0][0]);

    glBindVertexArray(mVao); // TODO(Ben): This wont work with all custom shaders?
    if (mLastUsedProgram != &program) {
        mLastUsedProgram = &program;

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(BasicVertex), (void*)offsetof(BasicVertex, pos));
        glVertexAttribPointer(program.getAttribute("vUV"), 2, GL_FLOAT, false, sizeof(BasicVertex), (void*)offsetof(BasicVertex, uvs));
        glVertexAttribPointer(program.getAttribute("vTint"), 4, GL_UNSIGNED_BYTE, true, sizeof(BasicVertex), (void*)offsetof(BasicVertex, color));
        glVertexAttribPointer(program.getAttribute("vAtlasPage"), 1, GL_UNSIGNED_BYTE, true, sizeof(BasicVertex), (void*)offsetof(BasicVertex, atlasPage));
    }

    // TODO: Maybe use  multiple texture units? Not bind this every time?
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(program.getUniform("tex"), 0);

    // Optional time
    if (const VGUniform* timeUniform = program.tryGetUniform("time")) {
        glUniform1f(*timeUniform, (float)sTotalTimeSeconds);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, mTexture);
    vg::SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D_ARRAY);

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);

    glBindVertexArray(0);
}

