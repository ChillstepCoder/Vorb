#include "stdafx.h"
#include "Skybox.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"
#include "camera/ICamera.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderDef.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/mesh/MeshDrawer.h"

#include "resources/ResourceManager.h"
#include "resources/CubemapRepository.h"
#include "resources/TextureRepository.h"
#include "resources/IAssetRepository.h"

#include "definitions/rendering/CubemapDef.h"

#include "options/LightingOptions.h"
#include "options/DebugOptions.h"

Skybox::~Skybox() {

}

void Skybox::init(const MaterialShaderDef* material, AssetHandlePtr<CubemapDef>&& skyCubemap) {
    constexpr unsigned NUM_VERTS = 4 * 6;
    constexpr float RADIUS = 1.0f;
    constexpr float DIAMETER = RADIUS * 2.0f;
    mMaterial = material;
    mMaterialPbr = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("sky_pbr");
    mSkyCubemap = std::move(skyCubemap);
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
    const MaterialDesc dummyMaterial;
    meshBuilder.addAxisAlignedQuad(bottomLeft, dims, CubeFacing::RIGHT, dummyMaterial, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft, dims, CubeFacing::BACK, dummyMaterial, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft + f32v3(DIAMETER, 0.0f, 0.0f), dims, CubeFacing::LEFT, dummyMaterial, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft + f32v3(0.0f, DIAMETER, 0.0f), dims, CubeFacing::FRONT, dummyMaterial, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft + f32v3(0.0f, 0.0f, DIAMETER), dims, CubeFacing::BOTTOM, dummyMaterial, uvRect, COLOR_WHITE);
    meshBuilder.addAxisAlignedQuad(bottomLeft, dims, CubeFacing::TOP, dummyMaterial, uvRect, COLOR_WHITE);
    mSkyboxMesh = std::make_unique<Mesh>();
    meshBuilder.finishMesh(mSkyboxMesh, f32v3(0.0f));
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

void Skybox::render(const f32m4& cameraMatrix) {
    if (!mSkyCubemap) return;
    const CubemapDef* cubemapDef = mSkyCubemap->tryGetAsset();
    if (!cubemapDef) return;

    glEnable(GL_DEPTH_CLAMP);
    assert(mMaterial);
    vg::DepthState::READ.set();
    glDepthFunc(GL_LEQUAL);

    ui32 textureUnit;
    MaterialRenderer::bindMaterialShaderForRender(*mMaterial, &textureUnit);
    glUniform1i(mMaterial->getUniform("unSkyboxCube"), textureUnit);
    glUniformMatrix4fv(mMaterial->getUniform("unVP"), 1, false, &cameraMatrix[0][0]);
    glBindTextureUnit(textureUnit, cubemapDef->getTexture());
    MeshDrawer::draw(mSkyboxMesh->mMainMesh);

    vg::DepthState::restorePrevious();
    glDisable(GL_DEPTH_CLAMP);
}

void Skybox::renderPbr(const f32m4& cameraMatrix) {
    if (!mSkyCubemap) return;
    const CubemapDef* cubemapDef = mSkyCubemap->tryGetAsset();
    if (!cubemapDef) return;

    glEnable(GL_DEPTH_CLAMP);
    assert(mMaterialPbr);
    vg::DepthState::READ.set();
    glDepthFunc(GL_LEQUAL);

    ui32 textureUnit;
    MaterialRenderer::bindMaterialShaderForRender(*mMaterialPbr, &textureUnit);
    glUniform1i(mMaterialPbr->getUniform("unSkyboxCube"), textureUnit);
    glUniformMatrix4fv(mMaterialPbr->getUniform("unVP"), 1, false, &cameraMatrix[0][0]);
    glBindTextureUnit(textureUnit, cubemapDef->getTexture());

    LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
    LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
    glUniform2f(mMaterialPbr->getUniform("unExposure"), optionsLeft.mExposure, optionsRight.mExposure);
    glUniform2f(mMaterialPbr->getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
    glUniform2f(mMaterialPbr->getUniform("unGamma"), optionsLeft.mGamma, optionsRight.mGamma);
    if (sDebugOptions.mLightPresetSplitView) {
        glUniform1f(mMaterialPbr->getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
    }
    else {
        glUniform1f(mMaterialPbr->getUniform("unLightingSplit"), 1.0f);
    }
    MeshDrawer::draw(mSkyboxMesh->mMainMesh);

    vg::DepthState::restorePrevious();
    glDisable(GL_DEPTH_CLAMP);
}

void Skybox::renderIrradianceDebug(const f32m4& cameraMatrix) {
    if (!mSkyCubemap) return;
    const CubemapDef* cubemapDef = mSkyCubemap->tryGetAsset();
    if (!cubemapDef) return;

    glEnable(GL_DEPTH_CLAMP);
    assert(mMaterial);
    vg::DepthState::READ.set();
    glDepthFunc(GL_LEQUAL);

    ui32 textureUnit;
    MaterialRenderer::bindMaterialShaderForRender(*mMaterial, &textureUnit);
    glUniform1i(mMaterial->getUniform("unSkyboxCube"), textureUnit);
    glUniformMatrix4fv(mMaterial->getUniform("unVP"), 1, false, &cameraMatrix[0][0]);
    glBindTextureUnit(textureUnit, cubemapDef->getIrradianceTexture());
    MeshDrawer::draw(mSkyboxMesh->mMainMesh);

    vg::DepthState::restorePrevious();
    glDisable(GL_DEPTH_CLAMP);
}

void Skybox::renderPrecomputedMapDebug(const f32m4& cameraMatrix, int baseLevel) {
    if (!mSkyCubemap) return;
    const CubemapDef* cubemapDef = mSkyCubemap->tryGetAsset();
    if (!cubemapDef) return;

    glEnable(GL_DEPTH_CLAMP);
    assert(mMaterial);
    vg::DepthState::READ.set();
    glDepthFunc(GL_LEQUAL);

    ui32 textureUnit;
    MaterialRenderer::bindMaterialShaderForRender(*mMaterial, &textureUnit);
    glUniform1i(mMaterial->getUniform("unSkyboxCube"), textureUnit);
    glUniformMatrix4fv(mMaterial->getUniform("unVP"), 1, false, &cameraMatrix[0][0]);
    VGTexture texture = cubemapDef->getPrefilterMap();
    glTextureParameteri(texture, GL_TEXTURE_BASE_LEVEL, baseLevel);
    glBindTextureUnit(textureUnit, texture);
    MeshDrawer::draw(mSkyboxMesh->mMainMesh);
    glTextureParameteri(texture, GL_TEXTURE_BASE_LEVEL, 0);

    vg::DepthState::restorePrevious();
    glDisable(GL_DEPTH_CLAMP);
}

void Skybox::setCubemap(AssetHandlePtr<CubemapDef>&& skyCubemap) {
    mSkyCubemap = std::move(skyCubemap);
}
