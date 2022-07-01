#include "stdafx.h"
#include "StaticPhysicsMesh.h"

#include "BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"

const f32v2 CUBE_FACING_AXIS_DIRECTIONS[e_cast(CubeFacing::COUNT)] = {
    f32v2(-1, 1), // LEFT
    f32v2(1,  1),  // FRONT
    f32v2(1,  1),  // RIGHT
    f32v2(-1, 1), // BACK
    f32v2(1,  1),  // TOP
    f32v2(-1, -1)   // BOTTOM
};
const f32v2 CUBE_FACING_AXIS_INITIAL_OFFSETS[e_cast(CubeFacing::COUNT)] = {
    f32v2(1, 0), // LEFT
    f32v2(0, 0),  // FRONT
    f32v2(0, 0),  // RIGHT
    f32v2(1, 0), // BACK
    f32v2(0, 0),  // TOP
    f32v2(1, 1)   // BOTTOM
};


StaticPhysicsMesh::StaticPhysicsMesh()
{

}

StaticPhysicsMesh::~StaticPhysicsMesh()
{

}

void StaticPhysicsMesh::reserveQuadCount(ui32 count)
{
    mVerts.reserve(4 * count);
    mIndices.reserve(6 * count);
}

void StaticPhysicsMesh::addTileQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis) {
    const size_t v = mVerts.size();
    const size_t ind = mIndices.size();
    mIndices.resize(ind + 6u);
    mIndices[ind] = v;
    mIndices[ind + 1u] = v + 1u;
    mIndices[ind + 2u] = v + 2u;
    mIndices[ind + 3u] = v + 2u;
    mIndices[ind + 4u] = v + 3u;
    mIndices[ind + 5u] = v;


    mVerts.resize(mVerts.size() + 4);

    f32v3* verts = (&mVerts.back() - 3);

    const i32v2& xyAxis = CUBE_FACING_AXIS[e_cast(axis)];
    const f32v2& xyAxisDirection = CUBE_FACING_AXIS_DIRECTIONS[e_cast(axis)];
    const f32v2& initialOffsetMult = CUBE_FACING_AXIS_INITIAL_OFFSETS[e_cast(axis)];


    // Offset for back faces so we can invert direction and have proper back face culling
    tilePosition[xyAxis.x] += xyDims.x * initialOffsetMult.x;
    tilePosition[xyAxis.y] += xyDims.y * initialOffsetMult.y;

    { // Bottom Left
        verts[0] = tilePosition;
    }
    { // Bottom Right
        verts[1] = tilePosition;
        verts[1][xyAxis.x] += (xyDims.x) * xyAxisDirection.x;
    }
    { // Top Right
        verts[2] = tilePosition;
        verts[2][xyAxis.x] += (xyDims.x) * xyAxisDirection.x;
        verts[2][xyAxis.y] += (xyDims.y) * xyAxisDirection.y;
    }
    { // Top Left
        verts[3] = tilePosition;
        verts[3][xyAxis.y] += (xyDims.y) * xyAxisDirection.y;
    }
}

void StaticPhysicsMesh::addQuadBetweenPoints(const f32v3 vertPoints[4]) {
    const size_t v = mVerts.size();
    const size_t ind = mIndices.size();
    mIndices.resize(ind + 6u);
    mIndices[ind] = v;
    mIndices[ind + 1u] = v + 1u;
    mIndices[ind + 2u] = v + 2u;
    mIndices[ind + 3u] = v + 2u;
    mIndices[ind + 4u] = v + 3u;
    mIndices[ind + 5u] = v;


    mVerts.resize(mVerts.size() + 4);

    f32v3* verts = (&mVerts.back() - 3);
    verts[0] = vertPoints[0];
    verts[1] = vertPoints[1];
    verts[2] = vertPoints[2];
    verts[3] = vertPoints[3];
}

void StaticPhysicsMesh::addTriangleBetweenPoints(const f32v3 vertPoints[3]) {
    const size_t v = mVerts.size();
    const size_t ind = mIndices.size();
    mIndices.resize(ind + 3u);
    mIndices[ind] = v;
    mIndices[ind + 1u] = v + 1u;
    mIndices[ind + 2u] = v + 2u;

    mVerts.resize(mVerts.size() + 3);

    f32v3* verts = (&mVerts.back() - 2);
    verts[0] = vertPoints[0];
    verts[1] = vertPoints[1];
    verts[2] = vertPoints[2];
}

void StaticPhysicsMesh::finish() {
    if (!mVerts.size()) {
        if (mPhysicsMesh) {
            mPhysicsMesh.reset();
        }
        return;
    }
    mPhysicsMesh = std::make_unique<btTriangleIndexVertexArray>();
}
