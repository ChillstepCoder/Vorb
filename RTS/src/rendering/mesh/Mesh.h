#pragma once

enum class MeshFlags : ui8 {
    TRIANGLES,
    QUADS,
};
constexpr ui8 MESH_FLAGS_MIXED_POLYS = e_cast(MeshFlags::TRIANGLES) | e_cast(MeshFlags::QUADS);

class Mesh
{
public:
    // TODO: Try having the program define bindVertexAttribs, since the program knows its attributes, not the mesh
private:
    VGVertexArray       mVao = 0;
    VGBuffer            mVbo = 0;
    VGIndexBuffer       mIbo = 0;
    ui32                mIndexCount = 0; ///< Current capacity of mIbo
    mutable VGProgram   mLastUsedProgram = UINT32_MAX;
    BoundingSphere      mBoundingSphere;  ///< Optional
    ui16                mIndexType; // SHORT OR INT
    BitFlags<MeshFlags> mFlags;
};
