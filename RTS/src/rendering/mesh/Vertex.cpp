#include "stdafx.h"
#include "Vertex.h"

void StandardVertex::bindVertexAttribs(VGBuffer vao) {
    assert(vao);
    // Standard verts
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(StandardVertex, pos));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 2 /*size*/, GL_FLOAT, false, offsetof(StandardVertex, uvs));
    glVertexArrayAttribBinding(vao, 1, 0);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribIFormat(vao, 2 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, offsetof(StandardVertex, textureIndex));
    glVertexArrayAttribBinding(vao, 2, 0);

    glEnableVertexArrayAttrib(vao, 3);
    glVertexArrayAttribFormat(vao, 3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, offsetof(StandardVertex, color));
    glVertexArrayAttribBinding(vao, 3, 0);

    glEnableVertexArrayAttrib(vao, 4);
    glVertexArrayAttribFormat(vao, 4 /*index*/, 3 /*size*/, GL_BYTE, false, offsetof(StandardVertex, normal));
    glVertexArrayAttribBinding(vao, 4, 0);

    glEnableVertexArrayAttrib(vao, 5);
    glVertexArrayAttribFormat(vao, 5 /*index*/, 2 /*size*/, GL_BYTE, false, offsetof(StandardVertex, tangent));
    glVertexArrayAttribBinding(vao, 5, 0);
    //glVertexArrayAttribFormat(vao, 6 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StandardVertex), (void*)offsetof(StandardVertex, windInfluence));
}

void TerrainVertex::bindVertexAttribs(VGBuffer vao) {
    assert(vao);

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(TerrainVertex, pos));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(TerrainVertex, normal));
    glVertexArrayAttribBinding(vao, 1, 0);
}

void WaterVertex::bindVertexAttribs(VGBuffer vao) {
    assert(vao);

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(WaterVertex, pos));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 1 /*size*/, GL_FLOAT, false, offsetof(WaterVertex, depth));
    glVertexArrayAttribBinding(vao, 1, 0);
}

void StaticModelVertex::bindVertexAttribs(VGBuffer vao) {
    assert(vao);
    // Standard verts
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(StaticModelVertex, pos));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 2 /*size*/, GL_UNSIGNED_SHORT, true, offsetof(StaticModelVertex, uvsPacked));
    glVertexArrayAttribBinding(vao, 1, 0);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribIFormat(vao, 2 /*index*/, 1 /*size*/, GL_UNSIGNED_INT, offsetof(StaticModelVertex, materialIndex));
    glVertexArrayAttribBinding(vao, 2, 0);

    glEnableVertexArrayAttrib(vao, 3);
    glVertexArrayAttribFormat(vao, 3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, offsetof(StaticModelVertex, color));
    glVertexArrayAttribBinding(vao, 3, 0);

    // https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
    glEnableVertexArrayAttrib(vao, 4);
    glVertexArrayAttribFormat(vao, 4 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof(StaticModelVertex, normalPacked));
    glVertexArrayAttribBinding(vao, 4, 0);

    glEnableVertexArrayAttrib(vao, 5);
    glVertexArrayAttribFormat(vao, 5 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof(StaticModelVertex, tangentPacked));
    glVertexArrayAttribBinding(vao, 5, 0);

   // glEnableVertexArrayAttrib(vao, .*);
    //assert(false && "Check that size in the shader is 3, in standard_tile it is 2");
    //glVertexArrayAttribFormat(vao, 4 /*index*/, 3 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, normalPacked));
   // glVertexArrayAttribFormat(vao, 5 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, tangentPacked));
}
