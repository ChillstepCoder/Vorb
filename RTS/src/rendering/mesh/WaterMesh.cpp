#include "stdafx.h"
#include "WaterMesh.h"

#include "rendering/RenderStats.h"

#include <Vorb/graphics/GLProgram.h>

VGBuffer WaterMesh::sWaterIbo;

constexpr ui32 WATER_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6;

void WaterMesh::beginMesh(const f32v2& cornerPos, f32 totalWidth)
{
    // TODO: recycle?
    // TODO: Do we need this pass?
    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;
    mVertexData.resize(TERRAIN_MESH_SIZE_VERTS);
    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            WaterVertex& v = mVertexData[y * TERRAIN_MESH_WIDTH_VERTS + x];
            v.pos.x = cornerPos.x + x * quadWidth;
            v.pos.y = cornerPos.y + y * quadWidth;
        }
    }
}

void WaterMesh::setVertsFromPaddedHeightfield(const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS])
{
    for (int y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            WaterVertex& v = mVertexData[y * TERRAIN_MESH_WIDTH_VERTS + x];
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.pos.z = 0.0f;
            v.depth = glm::max(-height, 0.0f);
        }
    }
}

void WaterMesh::draw(const vg::GLProgram& program) const
{
    if (!mIndexCount) return;
    assert(mVao);

    glBindVertexArray(mVao);
    bindVertexAttribs(program);

    // TODO: This material is currently broken as it both reads and writes to the normal buffer
    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);
}

void WaterMesh::finishMesh(MeshDrawMode drawMode)
{
    // Upload data
    if (mVertexData.size()) {

        lazyInitBuffers();

        mIndexCount = WATER_MESH_INDICES;
        const unsigned bufferSizeBytes = mVertexData.size() * sizeof(WaterVertex);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        // Orphan the buffer for speed
        glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
        // Set data
        glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, mVertexData.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(mVao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sWaterIbo);
        glBindVertexArray(0);
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
    std::vector<WaterVertex>().swap(mVertexData);
}

void WaterMesh::initGlobalIBO()
{
    if (sWaterIbo) {
        return;
    }

    std::vector<ui32> indices(WATER_MESH_INDICES);
    ui32 index = 0;
    for (int y = 0; y < TERRAIN_MESH_WIDTH_QUADS; y++) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_QUADS; x++) {
            // Compute index of back left vertex
            ui32 vertIndex = y * TERRAIN_MESH_WIDTH_VERTS + x;
            // Change triangle orientation based on odd or even
            if ((x + y) % 2) {
                indices[index++] = vertIndex;
                indices[index++] = vertIndex + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex;
            }
            else {
                indices[index++] = vertIndex + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS + 1;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex + TERRAIN_MESH_WIDTH_VERTS;
                indices[index++] = vertIndex;
                indices[index++] = vertIndex + 1;
            }
        }
    }

    assert(index == WATER_MESH_INDICES);

    glGenBuffers(1, &sWaterIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sWaterIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, WATER_MESH_INDICES * sizeof(ui32), indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    checkGlError("WaterMesh::initGlobalIBO");
}

void WaterMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != program.getID()) {
        mLastUsedProgram = program.getID();

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, pos));
        if (const VGAttribute* depthAttribute = program.tryGetAttribute("vDepth")) {
            glVertexAttribPointer(*depthAttribute, 1, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, depth));
        }
    }
    checkGlError("WaterMesh::bindVertexAttribs");
}