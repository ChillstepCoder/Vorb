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

void StaticModelVertex::bindVertexAttribs() {
    // Standard verts
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1 /*index*/, 2 /*size*/, GL_UNSIGNED_SHORT, true, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, uvsPacked));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, textureIndex));
    //glEnableVertexAttribArray(3);
    //glVertexAttribPointer(3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, color));
    
    // TODO: figure out 4 component GL_INT_2_10_10_10_REV!!!  https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
   // glEnableVertexAttribArray(4);
   // glVertexAttribPointer(4 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, normalPacked));
   // glEnableVertexAttribArray(5);
    //assert(false && "Check that size in the shader is 3, in standard_tile it is 2");
    //glVertexAttribPointer(4 /*index*/, 3 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, normalPacked));
   // glVertexAttribPointer(5 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, tangentPacked));
}
