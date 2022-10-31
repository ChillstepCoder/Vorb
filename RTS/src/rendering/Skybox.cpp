#include "stdafx.h"
#include "Skybox.h"

#include "rendering/QuadMesh.h"
#include "rendering/TileVertex.h"
#include "camera/ICamera.h"
#include "rendering/MaterialRenderer.h"

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
        const i32v2& axis = CUBE_FACING_AXIS[f];
        f32v3 bottomLeft(-RADIUS);
        bottomLeft += CUBE_FACING_GEOMETRY_OFFSETS[f] * DIAMETER;
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

        { // Top Right
            TileVertex& vtr = *(v++);
            vtr.pos = bottomLeft;
            vtr.uvs.x = 1.0f;
            vtr.uvs.y = 0.0f;
            vtr.pos[axis.x] += DIAMETER;
            vtr.pos[axis.y] += DIAMETER;
        }
        { // Top Left
            TileVertex& vtl = *(v++);
            vtl.pos = bottomLeft;
            vtl.uvs.x = 0.0f;
            vtl.uvs.y = 0.0f;
            vtl.pos[axis.y] += DIAMETER;
        }
    }

    mSkyboxMesh = std::make_unique<QuadMesh>();
    mSkyboxMesh->setData(verts, NUM_VERTS, MeshDrawMode::STATIC);

}

void Skybox::render() {
    assert(mMaterial);
    vg::DepthState::READ.set();
    MaterialRenderer::renderMesh(*mSkyboxMesh, *mMaterial);

    vg::DepthState::restorePrevious();
}
