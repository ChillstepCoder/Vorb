#include "stdafx.h"
#include "Skybox.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/ProceduralMeshBuilder.h"
#include "camera/ICamera.h"
#include "rendering/MaterialRenderer.h"

#include "resources/ResourceManager.h"
#include "resources/TextureRepository.h"

Skybox::~Skybox() {

}

void Skybox::init(const Material* material) {
    constexpr unsigned NUM_VERTS = 4 * 6;
    constexpr float RADIUS = 100000.0f;
    constexpr float DIAMETER = RADIUS * 2.0f;
    mMaterial = material;
    ProceduralMeshBuilder meshBuilder(true);
    // Bottom left

  /*  LEFT,
        FRONT,
        RIGHT,
        BACK,
        TOP,
        BOTTOM,*/
    // Left
    const f32v2 dims(DIAMETER);
    const f32v4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    f32v3 bottomLeft(-RADIUS);
    const SubTexture& dummyTexture = Services::ResourceManager::ref().getTexture("graycloud_bk");
    meshBuilder.addAxisAlignedQuad(bottomLeft, dims, CubeFacing::LEFT, dummyTexture, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft, dims, CubeFacing::FRONT, dummyTexture, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft + f32v3(DIAMETER, 0.0f, 0.0f), dims, CubeFacing::RIGHT, dummyTexture, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft + f32v3(0.0f, DIAMETER, 0.0f), dims, CubeFacing::BACK, dummyTexture, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft + f32v3(0.0f, 0.0f, DIAMETER), dims, CubeFacing::TOP, dummyTexture, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft, dims, CubeFacing::BOTTOM, dummyTexture, uvRect, COLOR_WHITE);
    mSkyboxMesh = std::make_unique<Mesh>();
    meshBuilder.finishMesh(mSkyboxMesh, MeshDrawMode::STATIC, f32v3(0.0f));
    //TileVertex verts[NUM_VERTS];
    //TileVertex* v = verts;

    //for (unsigned f = 0; f < 6; ++f) {
    //    const i32v2& axis = CUBE_FACING_AXIS[f];
    //    bottomLeft += CUBE_FACING_GEOMETRY_OFFSETS[f] * DIAMETER;
    //    { // Bottom Left
    //        TileVertex& vbl = *(v++);
    //        vbl.pos = bottomLeft;
    //        vbl.uvs.x = 0.0f;
    //        vbl.uvs.y = 1.0f;
    //    }
    //    { // Bottom Right
    //        TileVertex& vbr = *(v++);
    //        vbr.pos = bottomLeft;
    //        vbr.uvs.x = 1.0f;
    //        vbr.uvs.y = 1.0f;
    //        vbr.pos[axis.x] += DIAMETER;
    //    }

    //    { // Top Right
    //        TileVertex& vtr = *(v++);
    //        vtr.pos = bottomLeft;
    //        vtr.uvs.x = 1.0f;
    //        vtr.uvs.y = 0.0f;
    //        vtr.pos[axis.x] += DIAMETER;
    //        vtr.pos[axis.y] += DIAMETER;
    //    }
    //    { // Top Left
    //        TileVertex& vtl = *(v++);
    //        vtl.pos = bottomLeft;
    //        vtl.uvs.x = 0.0f;
    //        vtl.uvs.y = 0.0f;
    //        vtl.pos[axis.y] += DIAMETER;
    //    }
    //}

    //mSkyboxMesh = std::make_unique<QuadMesh>();
    //mSkyboxMesh->setData(verts, NUM_VERTS, MeshDrawMode::STATIC);

}

void Skybox::render() {
    assert(mMaterial);
    vg::DepthState::READ.set();
    MaterialRenderer::renderMesh(*mSkyboxMesh, *mMaterial);

    vg::DepthState::restorePrevious();
}
