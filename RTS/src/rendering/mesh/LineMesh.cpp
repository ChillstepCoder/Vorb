#include "stdafx.h"
#include "LineMesh.h"

LineMesh::LineMesh() = default;
LineMesh::~LineMesh() {
    if (mVao) {
        glDeleteVertexArrays(1, &mVao);
        glDeleteBuffers(1, &mVbo);
    }
}

void LineMesh::initialize(const std::vector<LineVertex>& vertices) {
    if (mVao) {
        glDeleteBuffers(1, &mVbo);
    }
    else {
        glCreateVertexArrays(1, &mVao);
    }

    glCreateBuffers(1, &mVbo);
    if (vertices.size()) {
        glNamedBufferStorage(mVbo, sizeof(LineVertex) * vertices.size(), vertices.data(), 0);
        glVertexArrayVertexBuffer(mVao, 0, mVbo, 0, sizeof(LineVertex));

        glEnableVertexArrayAttrib(mVao, 0);
        glVertexArrayAttribFormat(mVao, 0 /*index*/, 3 /*size*/, GL_FLOAT, GL_FALSE, offsetof(LineVertex, pos));
        glVertexArrayAttribBinding(mVao, 0, 0);
        glEnableVertexArrayAttrib(mVao, 1);
        glVertexArrayAttribFormat(mVao, 1 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(LineVertex, color));
        glVertexArrayAttribBinding(mVao, 1, 0);
    }
}

void LineMesh::bind() {
    assert(mVao);
    glBindVertexArray(mVao);
}

void LineMesh::drawLineStrip(int start, ui32 count) const {
    assert(count);
    assert(mVao);
    glDrawArrays(GL_LINE_STRIP, start, count);
}

void LineMesh::drawPoints(int start, ui32 count) const {
    assert(count);
    assert(mVao);
    glDrawArrays(GL_POINTS, start, count);
}

void LineMesh::drawLines(int start, ui32 count) const {
    assert(count);
    assert(mVao);
    glDrawArrays(GL_LINES, start, count);
}
