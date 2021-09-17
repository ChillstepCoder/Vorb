#include "stdafx.h"
#include "QuadMesh.h"

#include "world/Chunk.h"
#include "rendering/TileVertex.h"
#include "rendering/RenderContext.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

// Define all possible templates for Mesh class
// Each function definition should be proceeded by this

VGBuffer MeshBase::sIbo = 0;

MeshBase::MeshBase() {
    init();
}

MeshBase::~MeshBase() {
    destroy();
}

void MeshBase::initStaticIBO() {
    if (sIbo) {
        return;
    }

    ui32 i = 0;
    std::vector<ui32> quadIndices(MAX_MESH_INDICES);
    for (ui32 v = 0; i < MAX_MESH_INDICES; v += 4u) {
        quadIndices[i++] = v;
        quadIndices[i++] = v + 2;
        quadIndices[i++] = v + 3;
        quadIndices[i++] = v + 3;
        quadIndices[i++] = v + 1;
        quadIndices[i++] = v;
    }

    glGenBuffers(1, &sIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_MESH_INDICES * sizeof(ui32), quadIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void MeshBase::init() {
    if (mVao == 0) { // Create VAO
        glGenVertexArrays(1, &mVao);
        glBindVertexArray(mVao);

        glGenBuffers(1, &mVbo);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        assert(sIbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIbo);

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

    glBindVertexArray(0);
}

void QuadMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != &program) {
        mLastUsedProgram = &program;

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(TileVertex), (void*)offsetof(TileVertex, pos));
        if (const VGAttribute* uvAttribute = program.tryGetAttribute("vUV")) {
            glVertexAttribPointer(*uvAttribute, 2, GL_FLOAT, false, sizeof(TileVertex), (void*)offsetof(TileVertex, uvs));
        }
        if (const VGAttribute* atlasAttribute = program.tryGetAttribute("vAtlasPage")) {
            glVertexAttribPointer(*atlasAttribute, 1, GL_UNSIGNED_SHORT, false, sizeof(TileVertex), (void*)offsetof(TileVertex, atlasPage));
        }
        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(TileVertex), (void*)offsetof(TileVertex, color));
        }
        if (const VGAttribute* windAttribute = program.tryGetAttribute("vWindInfluence")) {
            glVertexAttribPointer(program.getAttribute("vWindInfluence"), 1, GL_UNSIGNED_BYTE, true, sizeof(TileVertex), (void*)offsetof(TileVertex, windInfluence));
        }
    }
}

void BillboardMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != &program) {
        mLastUsedProgram = &program;

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, rootPos));
        glVertexAttribPointer(program.getAttribute("vXZOffset"), 2, GL_FLOAT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, xzOffset));
        glVertexAttribPointer(program.getAttribute("vUV"), 2, GL_FLOAT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, uvs));
        glVertexAttribPointer(program.getAttribute("vAtlasPage"), 1, GL_UNSIGNED_SHORT, false, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, atlasPage));
        glVertexAttribPointer(program.getAttribute("vWindInfluence"), 1, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, windInfluence));

        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, color));
        }
    }
}
