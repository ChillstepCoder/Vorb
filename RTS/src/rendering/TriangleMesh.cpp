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
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
    std::vector<TriangleVertex>().swap(mVertexData);
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
    if (mLastUsedProgram != program.getID()) {
        mLastUsedProgram = program.getID();

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

void SkinnedMesh::setIndices(const uint16_t* indices, int indexCount) {
    assert(indices && indexCount);

    // TODO: no reallocate? Optimize?
   
    if (mIbo == 0) {
        glGenBuffers(1, &mIbo);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(uint16_t), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    mIndexCount = indexCount;
}

void SkinnedMesh::draw(const vg::GLProgram& program) const { // Make sure we have been initialized
    assert(mVao);
    assert(mIbo);

    glBindVertexArray(mVao);
    bindVertexAttribs(program);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_SHORT, nullptr);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void SkinnedMesh::finishMesh(MeshDrawMode drawMode) {
    // Does nothing
    UNUSED(drawMode);
    assert(mVao);
    assert(mIbo);
}

void SkinnedMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    f32v3 pos;
    f32v3 normal;
    f32v3 tangent;
    f32v3 uvs;
    color4 color;
    ui8 roughness;
    ui8 padding[11];

    // TODO: can we not do this every time?
    if (mLastUsedProgram != program.getID()) {
        mLastUsedProgram = program.getID();

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(SkinnedModelVertex), (void*)offsetof(SkinnedModelVertex, pos));
        if (const VGAttribute* uvAttribute = program.tryGetAttribute("vUV")) {
            glVertexAttribPointer(*uvAttribute, 2, GL_FLOAT, false, sizeof(SkinnedModelVertex), (void*)offsetof(SkinnedModelVertex, uvs));
        }
        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(SkinnedModelVertex), (void*)offsetof(SkinnedModelVertex, color));
        }
        if (const VGAttribute* normalAttribute = program.tryGetAttribute("vNormal")) {
            glVertexAttribPointer(*normalAttribute, 3, GL_FLOAT, true, sizeof(SkinnedModelVertex), (void*)offsetof(SkinnedModelVertex, normal));
        }
        if (const VGAttribute* tangentAttribute = program.tryGetAttribute("vTangent")) {
            glVertexAttribPointer(*tangentAttribute, 3, GL_FLOAT, true, sizeof(SkinnedModelVertex), (void*)offsetof(SkinnedModelVertex, tangent));
        }
        if (const VGAttribute* boneIdsAttribute = program.tryGetAttribute("vBoneIds")) {
            glVertexAttribIPointer(*boneIdsAttribute, MAX_BONES_PER_VERTEX, GL_UNSIGNED_BYTE, sizeof(SkinnedModelVertex), (void*)offsetof(SkinnedModelVertex, boneIDs));
        }
        if (const VGAttribute* boneWeightsAttribute = program.tryGetAttribute("vBoneWeights")) {
            glVertexAttribPointer(*boneWeightsAttribute, MAX_BONES_PER_VERTEX, GL_FLOAT, false, sizeof(SkinnedModelVertex), (void*)offsetof(SkinnedModelVertex, boneWeights));
        }
    }
}
