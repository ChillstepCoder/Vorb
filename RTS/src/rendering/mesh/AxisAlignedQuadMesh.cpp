#include "stdafx.h"
#include "AxisAlignedQuadMesh.h"

AxisAlignedQuadMesh::AxisAlignedQuadMesh() = default;

AxisAlignedQuadMesh::~AxisAlignedQuadMesh() {
    if (mVao) {
        glDeleteVertexArrays(1, &mVao);
        glDeleteBuffers(1, &mSsbo);
    }
}

void AxisAlignedQuadMesh::initialize(const std::vector<AxisAlignedQuadData>& quads) {
    mNumQuads = quads.size();
    if (mVao) {
        glDeleteBuffers(1, &mSsbo);
    }
    else {
        glCreateVertexArrays(1, &mVao);
    }

    glCreateBuffers(1, &mSsbo);
    if (mNumQuads) {
        glNamedBufferStorage(mSsbo, sizeof(AxisAlignedQuadData) * mNumQuads, quads.data(), 0);
    }
}

void AxisAlignedQuadMesh::bind(GLuint ssboIndex) {
    assert(mVao);
    glBindVertexArray(mVao);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, ssboIndex, mSsbo);
}

void AxisAlignedQuadMesh::drawQuads() {
    assert(mVao);
    if (mNumQuads) {
        glDrawArrays(GL_TRIANGLES, 0, mNumQuads * 6);
    }
}
