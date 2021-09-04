#include "stdafx.h"
#include "Skybox.h"

#include "rendering/QuadMesh.h"
#include "rendering/TileVertex.h"
#include "camera/ICamera.h"
#include "rendering/MaterialRenderer.h"

const i32v2 QUAD_FACING_AXIS[6] = {
    i32v2(AXIS_Y, AXIS_Z), // LEFT
    i32v2(AXIS_X, AXIS_Z),  // FRONT
    i32v2(AXIS_Y, AXIS_Z),  // RIGHT
    i32v2(AXIS_X, AXIS_Z), // BACK
    i32v2(AXIS_X, AXIS_Y),  // TOP
    i32v2(AXIS_X, AXIS_Y)   // BOTTOM
};

const i32v3 QUAD_FACING_ADJACENT_OFFSETS[6] = {
    i32v3(-1, 0, 0), // LEFT
    i32v3(0, -1, 0), // FRONT
    i32v3(1, 0, 0), // RIGHT
    i32v3(0, 1, 0), // BACK
    i32v3(0, 0, 1),  // TOP
    i32v3(0, 0, -1)  // BOTTOM
};

const f32v3 BOX_QUAD_FACING_GEOMETRY_OFFSETS[6] = {
    f32v3(0, 0, 0.0), // LEFT
    f32v3(0, 0, 0.0), // FRONT
    f32v3(1.0f, 0, 0.0), // RIGHT
    f32v3(0, 1.0f, 0.0), // BACK
    f32v3(0, 0, 1.0f),  // TOP
    f32v3(0, 0, 0.0) // BOTTOM
};

Skybox::~Skybox() {

}

void Skybox::init(const Material* material) {
    constexpr unsigned NUM_VERTS = 4 * 6;
    constexpr float RADIUS = 100000.0f;
    constexpr float DIAMETER = RADIUS * 2.0f;
    mMaterial = material;
    TileVertex verts[NUM_VERTS];
    TileVertex* v = verts;


    for (unsigned f = 0; f < 6; ++f) {
        const i32v2& axis = QUAD_FACING_AXIS[f];
        f32v3 bottomLeft(-RADIUS);
        bottomLeft += BOX_QUAD_FACING_GEOMETRY_OFFSETS[f] * DIAMETER;
        { // Bottom Left
            TileVertex& vbl = *(v++);
            vbl.pos = bottomLeft;
            vbl.uvs.x = 0.0f;
            vbl.uvs.y = 1.0f;
        }
        { // Bottom Right
            TileVertex& vbr = *(v++);
            vbr.pos = bottomLeft;
            vbr.uvs.x = 1.0f;
            vbr.uvs.y = 1.0f;
            vbr.pos[axis.x] += DIAMETER;
        }

        { // Top Left
            TileVertex& vtl = *(v++);
            vtl.pos = bottomLeft;
            vtl.uvs.x = 0.0f;
            vtl.uvs.y = 0.0f;
            vtl.pos[axis.y] += DIAMETER;
        }
        { // Top Right
            TileVertex& vtr = *(v++);
            vtr.pos = bottomLeft;
            vtr.uvs.x = 1.0f;
            vtr.uvs.y = 0.0f;
            vtr.pos[axis.x] += DIAMETER;
            vtr.pos[axis.y] += DIAMETER;
        }
    }

    mSkyboxMesh = std::make_unique<QuadMesh>();
    mSkyboxMesh->setData(verts, NUM_VERTS, QuadMeshDrawMode::STATIC);

}

void Skybox::render(const MaterialRenderer& materialRenderer) {
    assert(mMaterial);
    vg::DepthState::READ.set();
    materialRenderer.renderMesh(*mSkyboxMesh, *mMaterial);

    vg::DepthState::restorePrevious();
}
