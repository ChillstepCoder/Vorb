#include "stdafx.h"
#include "TerrainMesh.h"

#include "rendering/RenderStats.h"

VGBuffer TerrainMesh::sTerrainIbo;

#include <Vorb/graphics/GLProgram.h>

constexpr ui32 TERRAIN_MESH_INDICES = SQ(TERRAIN_MESH_WIDTH_QUADS) * 6;

void TerrainMesh::beginMesh(const f32v2& cornerPos, f32 totalWidth) {
    // TODO: recycle?
    const f32 quadWidth = totalWidth / TERRAIN_MESH_WIDTH_QUADS;
    mVertexData.resize(TERRAIN_MESH_SIZE_VERTS);
    for (ui32 y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (ui32 x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            TerrainVertex& v = mVertexData[y * TERRAIN_MESH_WIDTH_VERTS + x];
            v.pos.x = cornerPos.x + x * quadWidth;
            v.pos.y = cornerPos.y + y * quadWidth;
        }
    }
}

void TerrainMesh::setVertsFromPaddedHeightfield(const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]) {
    for (int y = 0; y < TERRAIN_MESH_WIDTH_VERTS; ++y) {
        for (int x = 0; x < TERRAIN_MESH_WIDTH_VERTS; ++x) {
            TerrainVertex& v = mVertexData[y * TERRAIN_MESH_WIDTH_VERTS + x];
            f32 height = paddedHeightfield[y + 1][x + 1];
            v.pos.z = height;
            // Normal calc
            f32 front = paddedHeightfield[y][x + 1];
            f32 back = paddedHeightfield[y + 2][x + 1];
            f32 left = paddedHeightfield[y + 1][x];
            f32 right = paddedHeightfield[y + 1][x + 2];

            f32v3 normal = glm::normalize(f32v3(2.0f * (right - left), 2.0f * (back - front), 4.0f));
            v.normal = normal;
        }
    }
}

void TerrainMesh::draw(const vg::GLProgram& program) const
{
    if (!mIndexCount) return;
    assert(mVao);

    glBindVertexArray(mVao);
    bindVertexAttribs(program);

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mIndexCount / 3);

    glBindVertexArray(0);
}

void TerrainMesh::finishMesh(MeshDrawMode drawMode) {

    // Upload data
    if (mVertexData.size()) {

        lazyInitBuffers();

        mIndexCount = TERRAIN_MESH_INDICES;
        const unsigned bufferSizeBytes = mVertexData.size() * sizeof(TerrainVertex);

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        // Orphan the buffer for speed
        glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, enum_cast(drawMode));
        // Set data
        glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, mVertexData.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        std::vector<TerrainVertex>().swap(mVertexData);

        glBindVertexArray(mVao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sTerrainIbo);
        glBindVertexArray(0);
    }
    else {
        destroy(); // Mesh is now empty, destroy if it was valid
    }
}


void TerrainMesh::bindVertexAttribs(const vg::GLProgram& program) const {
    // TODO: can we not do this every time?
    if (mLastUsedProgram != &program) {
        mLastUsedProgram = &program;

        glBindBuffer(GL_ARRAY_BUFFER, mVbo);

        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, pos));
        if (const VGAttribute* normalAttribute = program.tryGetAttribute("vNormal")) {
            glVertexAttribPointer(*normalAttribute, 3, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
        }
    }
    checkGlError("TerrainMesh::bindVertexAttribs");
}


void TerrainMesh::initGlobalIBO() {
    if (sTerrainIbo) {
        return;
    }

    std::vector<ui32> indices(TERRAIN_MESH_INDICES);
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
    assert(index == TERRAIN_MESH_INDICES);

    glGenBuffers(1, &sTerrainIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sTerrainIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, TERRAIN_MESH_INDICES * sizeof(ui32), indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    checkGlError("TerrainMesh::initGlobalIBO");
}
