#include "stdafx.h"
#include "Vertex.h"

void StandardVertex::bindVertexAttribs() {
    // Standard verts
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1 /*index*/, 2 /*size*/, GL_FLOAT, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, uvs));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, sizeof(StandardVertex), (void*)offsetof(StandardVertex, textureIndex));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StandardVertex), (void*)offsetof(StandardVertex, color));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4 /*index*/, 3 /*size*/, GL_BYTE, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, normal));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5 /*index*/, 2 /*size*/, GL_BYTE, false, sizeof(StandardVertex), (void*)offsetof(StandardVertex, tangent));
    //glVertexAttribPointer(6 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StandardVertex), (void*)offsetof(StandardVertex, windInfluence));
}

void TerrainVertex::bindVertexAttribs() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
}

void WaterVertex::bindVertexAttribs() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1 /*index*/, 1 /*size*/, GL_FLOAT, false, sizeof(WaterVertex), (void*)offsetof(WaterVertex, depth));
}
