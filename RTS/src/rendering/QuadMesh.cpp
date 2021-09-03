#include "stdafx.h"
#include "QuadMesh.h"

#include "world/Chunk.h"
#include "rendering/TileVertex.h"
#include "rendering/RenderContext.h"
#include "camera/Camera2D.h"

#include <Vorb/graphics/GLProgram.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/RasterizerState.h>

// Define all possible templates for Mesh class
// Each function definition should be proceeded by this

bool sQuadIndicesInitialized = false;
ui32 sQuadIndices[MAX_MESH_INDICES];

void initSharedQuadIndices() {
    ui32 i = 0;
    for (ui32 v = 0; i < MAX_MESH_INDICES; v += 4u) {
        sQuadIndices[i++] = v;
        sQuadIndices[i++] = v + 2;
        sQuadIndices[i++] = v + 3;
        sQuadIndices[i++] = v + 3;
        sQuadIndices[i++] = v + 1;
        sQuadIndices[i++] = v;
    }
    sQuadIndicesInitialized = true;
}

MeshBase::MeshBase() {
    init();
}

MeshBase::~MeshBase() {
    destroy();
}

void MeshBase::init() {
    if (mVao == 0) { // Create VAO
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

void MeshBase::destroy() {
    if (mVbo != 0) {
        glDeleteBuffers(1, &mVbo);
        mVbo = 0;
    }
    // TODO: Shared
    if (mIbo != 0) {
        glDeleteBuffers(1, &mIbo);
        mIbo = 0;
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

        if (const VGAttribute* tintAttribute = program.tryGetAttribute("vTint")) {
            glVertexAttribPointer(*tintAttribute, 4, GL_UNSIGNED_BYTE, true, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, color));
        }
    }
}
