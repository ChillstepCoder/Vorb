#include "stdafx.h"
#include "MeshBase.h"

#include "rendering/RenderStats.h"

VGBuffer MeshBase::sQuadIbo = 0;

MeshBase::MeshBase() {
}

MeshBase::~MeshBase() {
    destroy();
}

void MeshBase::initStaticIBO() {
    if (sQuadIbo) {
        return;
    }

    ui32 i = 0;
    std::vector<ui32> quadIndices(MAX_MESH_INDICES);
    for (ui32 v = 0; i < MAX_MESH_INDICES; v += 4u) {
        quadIndices[i++] = v;
        quadIndices[i++] = v + 1;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 3;
        quadIndices[i++] = v;
    }

    glGenBuffers(1, &sQuadIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sQuadIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_MESH_INDICES * sizeof(ui32), quadIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void MeshBase::lazyInitBuffers() {
    if (mVao == 0) { // Create VAO
        glGenVertexArrays(1, &mVao);
        glBindVertexArray(mVao);

        glGenBuffers(1, &mVbo);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        assert(sQuadIbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sQuadIbo);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void MeshBase::destroy() {
    if (mVbo != 0) {
        glDeleteBuffers(1, &mVbo);
        mVbo = 0;
    }
    if (mVao != 0) {
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
    }

    mIndexCount = 0;
}

void MeshBase::draw(const vg::GLProgram& program) const {
    // Make sure we have been initialized
    assert(mVao);
    if (!mIndexCount) return;

    glBindVertexArray(mVao);
    bindVertexAttribs(program);

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);
}
