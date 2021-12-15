#include "stdafx.h"
#include "TriangleMesh.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

#include "rendering/RenderStats.h"

void TriangleMesh::reserveTriangleCount(size_t count) {
    mVertexData.reserve(count * 3u);
}

void TriangleMesh::addTriangle(TriangleVertex verts[3]) {
    mVertexData.resize(mVertexData.size() + 3);
    memcpy(&mVertexData[mVertexData.size() - 3], verts, sizeof(TriangleVertex) * 3);
}

void TriangleMesh::finishMesh(MeshDrawMode drawMode) {
    if (mVertexData.size()) {
        setData(mVertexData.data(), mVertexData.size(), drawMode);
        std::vector<TriangleVertex>().swap(mVertexData);
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
}

void TriangleMesh::draw(const vg::GLProgram& program) const {
    // Make sure we have been initialized
    assert(mVao);

    glBindVertexArray(mVao);
    bindVertexAttribs(program);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glDrawArrays(GL_TRIANGLES, 0 /* first */, (GLsizei)mIndexCount);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);
}

void TriangleMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != &program) {
        mLastUsedProgram = &program;

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, pos));
        if (const VGAttribute* uvAttribute = program.tryGetAttribute("vUV")) {
            glVertexAttribPointer(*uvAttribute, 2, GL_FLOAT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, uvs));
        }
        if (const VGAttribute* uvTileAttribute = program.tryGetAttribute("vUVTiling")) {
            glVertexAttribPointer(*uvTileAttribute, 4, GL_FLOAT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, uvTiling));
        }
        if (const VGAttribute* atlasAttribute = program.tryGetAttribute("vAtlasPage")) {
            glVertexAttribPointer(*atlasAttribute, 1, GL_UNSIGNED_SHORT, false, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, atlasPage));
        }
        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, color));
        }
        if (const VGAttribute* normalAttribute = program.tryGetAttribute("vNormal")) {
            glVertexAttribPointer(*normalAttribute, 3, GL_FLOAT, true, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, normal));
        }
    }
}
