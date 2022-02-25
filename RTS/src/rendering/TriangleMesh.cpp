#include "stdafx.h"
#include "TriangleMesh.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

#include "rendering/RenderStats.h"

#include <assimp/scene.h>

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

void IndexedTriangleMesh::setFaces(const aiFace* faces, ui32 numFaces) {
    assert(faces && numFaces);
    assert(faces[0].mNumIndices == 3); // triangle only

    // TODO: no reallocate? Optimize?
    std::vector<ui32> indices;
    indices.resize(numFaces * 3u);
    ui32 index = 0;
    for (ui32 i = 0; i < numFaces; ++i) {
        indices[index++] = faces[i].mIndices[0];
        indices[index++] = faces[i].mIndices[1];
        indices[index++] = faces[i].mIndices[2];
    }
    if (mIbo == 0) {
        glGenBuffers(1, &mIbo);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(ui32), indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    mIndexCount = indices.size();
}

void IndexedTriangleMesh::draw(const vg::GLProgram& program) const { // Make sure we have been initialized
    assert(mVao);
    assert(mIbo);

    glBindVertexArray(mVao);
    bindVertexAttribs(program);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, nullptr);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void IndexedTriangleMesh::finishMesh(MeshDrawMode drawMode) {
    // Does nothing
    UNUSED(drawMode);
    assert(mVao);
    assert(mIbo);
}

void IndexedTriangleMesh::bindVertexAttribs(const vg::GLProgram& program) const {
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
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(ModelVertex), (void*)offsetof(ModelVertex, pos));
        if (const VGAttribute* uvAttribute = program.tryGetAttribute("vUV")) {
            glVertexAttribPointer(*uvAttribute, 3, GL_FLOAT, false, sizeof(ModelVertex), (void*)offsetof(ModelVertex, uvs));
        }
        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(ModelVertex), (void*)offsetof(ModelVertex, color));
        }
        if (const VGAttribute* normalAttribute = program.tryGetAttribute("vNormal")) {
            glVertexAttribPointer(*normalAttribute, 3, GL_FLOAT, true, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
        }
        if (const VGAttribute* tangentAttribute = program.tryGetAttribute("vTangent")) {
            glVertexAttribPointer(*tangentAttribute, 3, GL_FLOAT, true, sizeof(ModelVertex), (void*)offsetof(ModelVertex, tangent));
        }
        if (const VGAttribute* bitangentAttribute = program.tryGetAttribute("vBitangent")) {
            glVertexAttribPointer(*bitangentAttribute, 3, GL_FLOAT, true, sizeof(ModelVertex), (void*)offsetof(ModelVertex, bitangent));
        }
        if (const VGAttribute* roughnessAttribute = program.tryGetAttribute("vRoughness")) {
            glVertexAttribPointer(*roughnessAttribute, 1, GL_UNSIGNED_BYTE, true, sizeof(ModelVertex), (void*)offsetof(ModelVertex, roughness));
        }
    }
}
