#include "stdafx.h"
#include "Vertex.h"

VertexType TerrainVertex::bindVertexAttribs(VGBuffer vao) {
    assert(vao);

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(TerrainVertex, pos));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(TerrainVertex, normal));
    glVertexArrayAttribBinding(vao, 1, 0);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribFormat(vao, 2 /*index*/, 2 /*size*/, GL_FLOAT, false, offsetof(TerrainVertex, uvs));
    glVertexArrayAttribBinding(vao, 2, 0);

    return VertexType::TERRAIN;
}

VertexType WaterVertex::bindVertexAttribs(VGBuffer vao) {
    assert(vao);

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(WaterVertex, pos));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 1 /*size*/, GL_FLOAT, false, offsetof(WaterVertex, depth));
    glVertexArrayAttribBinding(vao, 1, 0);

    return VertexType::WATER;
}

void StaticModelVertex::build(const f32v3& pos, const f32v3& normal, const f32v3& tangent, const f32v2& uvs, const color4& color, MaterialID materialId, ui8 windInfluence) {
    this->pos = pos;
    this->materialId = materialId;
    //assert(rawVert.uvs.x >= 0.0f && rawVert.uvs.x <= 1.0f && rawVert.uvs.y >= 0.0f && rawVert.uvs.y <= 1.0f);
    this->uvsPacked = PackUVs(uvs);
    this->normalPacked = Pack_INT_2_10_10_10_REV(normal.x, normal.y, normal.z, 0.0f);
    this->tangentPacked = Pack_INT_2_10_10_10_REV(tangent.x, tangent.y, tangent.z, 0.0f);
    this->color = color;
}

VertexType StaticModelVertex::bindVertexAttribs(VGBuffer vao) {
    assert(vao);
    //// Standard verts
    //glEnableVertexArrayAttrib(vao, 0);
    //glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(StaticModelVertex, pos));
    //glVertexArrayAttribBinding(vao, 0, 0);

    // TODO: Test out splitting positions into separate buffer 
    constexpr ui32 BINDING_POINT_POSITION = 0;
    constexpr ui32 BINDING_POINT_INTERLEAVED = 0;

    // TMP: Override positions binding to the non interleaved section of the buffer
        // Standard verts
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, 0);
    glVertexArrayAttribBinding(vao, 0, BINDING_POINT_POSITION);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 2 /*size*/, GL_SHORT, true, offsetof(StaticModelVertex, uvsPacked));
    glVertexArrayAttribBinding(vao, 1, BINDING_POINT_INTERLEAVED);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribIFormat(vao, 2 /*index*/, 1 /*size*/, GL_UNSIGNED_SHORT, offsetof(StaticModelVertex, materialId));
    glVertexArrayAttribBinding(vao, 2, BINDING_POINT_INTERLEAVED);

    glEnableVertexArrayAttrib(vao, 3);
    glVertexArrayAttribFormat(vao, 3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, offsetof(StaticModelVertex, color));
    glVertexArrayAttribBinding(vao, 3, BINDING_POINT_INTERLEAVED);

    // https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
    glEnableVertexArrayAttrib(vao, 4);
    glVertexArrayAttribFormat(vao, 4 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof(StaticModelVertex, normalPacked));
    glVertexArrayAttribBinding(vao, 4, BINDING_POINT_INTERLEAVED);

    glEnableVertexArrayAttrib(vao, 5);
    glVertexArrayAttribFormat(vao, 5 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof(StaticModelVertex, tangentPacked));
    glVertexArrayAttribBinding(vao, 5, BINDING_POINT_INTERLEAVED);

    // TODO: WIND

   // glEnableVertexArrayAttrib(vao, .*);
    //assert(false && "Check that size in the shader is 3, in standard_tile it is 2");
    //glVertexArrayAttribFormat(vao, 4 /*index*/, 3 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, normalPacked));
   // glVertexArrayAttribFormat(vao, 5 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, tangentPacked));
    return VertexType::STATIC_MODEL;
}

VertexType SkinnedModelVertex::bindVertexAttribs(VGBuffer vao)
{
    // Standard verts
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0 /*index*/, 3 /*size*/, GL_FLOAT, false, offsetof(SkinnedModelVertex, pos));
    glVertexArrayAttribBinding(vao, 0, 0);

    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 1 /*index*/, 2 /*size*/, GL_SHORT, true, offsetof(SkinnedModelVertex, uvsPacked));
    glVertexArrayAttribBinding(vao, 1, 0);

    glEnableVertexArrayAttrib(vao, 2);
    glVertexArrayAttribIFormat(vao, 2 /*index*/, 1 /*size*/, GL_UNSIGNED_SHORT, offsetof(SkinnedModelVertex, materialId));
    glVertexArrayAttribBinding(vao, 2, 0);

    glEnableVertexArrayAttrib(vao, 3);
    glVertexArrayAttribFormat(vao, 3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, offsetof(SkinnedModelVertex, color));
    glVertexArrayAttribBinding(vao, 3, 0);

    // https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
    glEnableVertexArrayAttrib(vao, 4);
    glVertexArrayAttribFormat(vao, 4 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof(SkinnedModelVertex, normalPacked));
    glVertexArrayAttribBinding(vao, 4, 0);

    glEnableVertexArrayAttrib(vao, 5);
    glVertexArrayAttribFormat(vao, 5 /*index*/, 4 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof(SkinnedModelVertex, tangentPacked));
    glVertexArrayAttribBinding(vao, 5, 0);

    glEnableVertexArrayAttrib(vao, 11);
    glVertexArrayAttribFormat(vao, 11 /*index*/, MAX_BONES_PER_VERTEX /*size*/, GL_FLOAT, false, offsetof(SkinnedModelVertex, boneWeights));
    glVertexArrayAttribBinding(vao, 11, 0);

    glEnableVertexArrayAttrib(vao, 12);
    glVertexArrayAttribIFormat(vao, 12 /*index*/, MAX_BONES_PER_VERTEX /*size*/, GL_UNSIGNED_BYTE, offsetof(SkinnedModelVertex, boneIDs));
    glVertexArrayAttribBinding(vao, 12, 0);

    return VertexType::SKINNED_MODEL;
}

constexpr size_t getVertexSize(VertexType type) {
    switch (type) {
        case VertexType::TERRAIN:
            return sizeof(TerrainVertex);
        case VertexType::WATER:
            return sizeof(WaterVertex);
        case VertexType::STATIC_MODEL:
            return sizeof(StaticModelVertex);
        case VertexType::SKINNED_MODEL:
            return sizeof(SkinnedModelVertex);
        default:
            assert(false);
    }
    return 0;
    static_assert(e_cast(VertexType::COUNT) == 5);
}
