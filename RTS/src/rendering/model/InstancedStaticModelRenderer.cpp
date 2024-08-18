#include "stdafx.h"
#include "InstancedStaticModelRenderer.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "resources/MaterialRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshDrawer.h"
#include "rendering/MaterialUtils.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/post_process/ShadowDetail.h"
#include "rendering/RenderContext.h"
#include "rendering/post_process/ShadowPassShaderData.h"
#include "rendering/model/InstancedStaticModelManager.h"
#include "options/DebugOptions.h"
#include "options/LightingOptions.h"
#include "rendering/material/BrdfLUT.h"

#include "definitions/rendering/CubemapDef.h"

#include "world/World.h"
#include "weather/WeatherManager.h"

#include "camera/Camera3D.h"

#include "rendering/gl/GL.h"


InstancedStaticModelRenderer::InstancedStaticModelRenderer() {

    mStandardShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("standard_model"));
    mStandardShaderNew = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("standard_model_new"));
    mShadowMapperShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("shadow_mapper_instd"));
    mSmudgeShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("smudge_model"));
    mSmudgeShaderNew = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("smudge_model_new"));
    mWaterShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("water_model"));

    static_assert(e_count(MaterialRenderPassType) == 3);
}

InstancedStaticModelRenderer::~InstancedStaticModelRenderer() = default;

void InstancedStaticModelRenderer::setActiveWorld(World& world) {
    mWeatherManager = &world.getWeatherManager();
}
void InstancedStaticModelRenderer::renderModelPass(const InstancedStaticModelManager& modelManager, const Camera3D& camera, MaterialRenderPassType passType, const CubemapDef* skyCubeMap)
{
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels) [[unlikely]] {
        return;
    }

    if (!modelManager.mDrawCommandsCount[e_cast(passType)]) {
        return;
    }

    if (!mShaderAssets.areAllAssetsLoaded()) [[unlikely]] {
        return;
    }

    GLDrawCommandBuffer& drawCommands = *modelManager.mDrawCommands[e_cast(passType)];
    if (!drawCommands.getNumActiveCommands()) {
        return;
    }
    PROFILE_FUNCTION();

    // TODO: Material specific, we lose 10fps disabling this
    glDisable(GL_CULL_FACE);
    const MaterialShaderDef* def = nullptr;

    switch (passType) {
        case MaterialRenderPassType::Default:
            def = mStandardShaderNew;
            break;
        case MaterialRenderPassType::Smudge:
            def = mSmudgeShaderNew;
            break;
        case MaterialRenderPassType::Water:
            def = mWaterShader;
            break;
        default:
            assert(false);
            break;

    }
    static_assert(e_count(MaterialRenderPassType) == 3);

    ui32 nextTextureIndex = 0;
    MaterialRenderer::bindMaterialShaderForRender(*def, &nextTextureIndex);
    VGUniform windUniform = 0;
    if (const VGUniform* uniform = def->tryGetUniform("unWindType")) {
        windUniform = *uniform;
    }

    if (passType == MaterialRenderPassType::Water) {
        // TODO: UBO?
        // Water uniforms
        //glUniform4fv(def->getUniform("unShallowColor"), 1, &sDebugOptions.mShallowWaterColor.x);
        //glUniform4fv(def->getUniform("unDeepColor"), 1, &sDebugOptions.mDeepWaterColor.x);
        glUniform4fv(def->getUniform("unFoamColor"), 1, &sDebugOptions.mWaterFoamColor.x);
        glUniform1f(def->getUniform("unSurfaceDistortAmount"), sDebugOptions.mWaterSurfaceDistortAmount);
        glUniform1f(def->getUniform("unSurfaceMoveSpeed"), sDebugOptions.mWaterSurfaceMoveSpeed);
        glUniform2fv(def->getUniform("unFoamDistanceRange"), 1, &sDebugOptions.mWaterFoamDistanceRange.x);
        glUniform1f(def->getUniform("unSurfaceNoiseCutoff"), sDebugOptions.mWaterSurfaceNoiseCutoff);
        glUniform1f(def->getUniform("unSmoothstepAA"), sDebugOptions.mWaterSmoothstepAA);
        glUniform1f(def->getUniform("unColorNoiseIntensity"), sDebugOptions.mWaterColorNoiseIntensity);
        glUniform1f(def->getUniform("unDistortTiling"), sDebugOptions.mWaterDistortTiling);
        glUniform1f(def->getUniform("unNoiseTiling"), sDebugOptions.mWaterNoiseTiling);
        glUniform1f(def->getUniform("unWaterMetallic"), sDebugOptions.mWaterMetallic);
        glUniform1f(def->getUniform("unWaterRoughness"), sDebugOptions.mWaterRoughness);
        glUniform1i(def->getUniform("unIrradianceMap"), nextTextureIndex);
        glBindTextureUnit(nextTextureIndex++, skyCubeMap->getIrradianceTexture());
        glUniform1i(def->getUniform("unPrefilterMap"), nextTextureIndex);
        glBindTextureUnit(nextTextureIndex++, skyCubeMap->getPrefilterMap());
        glUniform1i(def->getUniform("unBrdfLUT"), nextTextureIndex);
        glBindTextureUnit(nextTextureIndex++, BrdfLUT::getTexture());

        LightingOptions& optionsLeft = *sDebugOptions.mLightingOptions;
        LightingOptions& optionsRight = *sDebugOptions.mLightingOptionsSplit;
        glUniform2f(def->getUniform("unAmbient"), optionsLeft.mAmbient, optionsRight.mAmbient);
        glUniform2f(def->getUniform("unExposure"), optionsLeft.mExposure, optionsRight.mExposure);
        glUniform2f(def->getUniform("unHazeExponent"), optionsLeft.mHazeExponent, optionsRight.mHazeExponent);
        if (sDebugOptions.mIsCameraUnderwater) {
            glUniform2f(def->getUniform("unHazeDivisor"), sDebugOptions.mUnderwaterHazeDivisor, sDebugOptions.mUnderwaterHazeDivisor);
        }
        else {
            glUniform2f(def->getUniform("unHazeDivisor"), optionsLeft.mHazeDivisor, optionsRight.mHazeDivisor);
        }
        glUniform2f(def->getUniform("unSunIntensity"), optionsLeft.mSunIntensity, optionsRight.mSunIntensity);
        if (sDebugOptions.mLightPresetSplitView) {
            glUniform1f(def->getUniform("unLightingSplit"), sDebugOptions.mLightPresetSplitAmount);
        }
        else {
            glUniform1f(def->getUniform("unLightingSplit"), 1.0f);
        }
    }

    glUniform1f(def->getUniform("unSnowLevel"), mWeatherManager->mSnowLevel);
    // Force load
    MaterialRepository::get().getAssetHandle(CStrToken("wood_chopping_texture_01"));
    if (def->tryGetUniform("unDamageTexture")) {
        glUniform1ui(def->getUniform("unDamageTexture"), MaterialRepository::get().getMaterialId(CStrToken("wood_chopping_texture_01")));
    }

    // TODO: Not uniform, instead per model when we have improved batching
    if (windUniform) {
        glUniform1i(windUniform, 0/*(GLint)mesh.getSubmeshData()->windType*/);
    }

    ModelRepository& modelRepo = ModelRepository::get();
    // Talia said we never need more than 65536 verts
    const ModelBatch& batch = modelRepo.getModelBatch(ModelBatchKey{ MeshIndexType::USHORT, VertexType::STANDARD_MODEL, passType });

    // Variant data
    //assert(mesh.mVariantDataUbo);
    // TODO: VARIANTS
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_SSBO, modelRepo.getModelVariantDataSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_DAMAGE_DATA_SSBO, modelManager.mDamageZonesSSBO);

    // Bind our transforms every frame as we could be using different instanced static model managers
    VGBuffer vao = batch.getVao();
    GL.glVertexArrayVertexBuffer(vao, MODEL_TRANSFORMS_BINDING_POINT, modelManager.mTransformsVbo, 0, sizeof(f32m4));
    GL.glVertexArrayVertexBuffer(vao, MODEL_VARIANT_INDICES_BINDING_POINT, modelManager.mVariantsVbo, 0, sizeof(ui32));
    GL.glVertexArrayVertexBuffer(vao, MODEL_DAMAGE_INDICES_BINDING_POINT, modelManager.mDamageModelIndexVbo, 0, sizeof(ui32));

    batch.bindStaticModelAttribs();

    GL.glBindVertexArray(vao);
    assert(batch.getIndexType() == MeshIndexType::USHORT);
    drawCommands.multiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT);

    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelPass");
}

void InstancedStaticModelRenderer::renderModelShadows(const InstancedStaticModelManager& modelManager, const ShadowPassShaderData& shaderData, const Camera3D& camera)
{
    ASSERT_RENDER_THREAD();

    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    if (sDebugOptions.mHideModels) [[unlikely]] {
        return;
    }

    // TODO: Material specific
    glDisable(GL_CULL_FACE);

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialShaderForRender(*mShadowMapperShader);
    glUniformMatrix4fv(mShadowMapperShader->getUniform("unShadowFrustumMatrices[0]"), MAX_SHADOW_CASCADE_LEVELS, false, &(*shaderData.shadowFrustumMatrices)[0][0]);

    ModelRepository& modelRepo = ModelRepository::get();
    // Talia said we never need more than 65536 verts
    for (MaterialRenderPassType passType : { MaterialRenderPassType::Default, MaterialRenderPassType::Smudge }) {
        if (!modelManager.mDrawCommandsShadows[e_cast(passType)]) {
            continue;
        }

        GLDrawCommandBuffer& drawCommands = *modelManager.mDrawCommandsShadows[e_cast(passType)];
        if (!drawCommands.getNumActiveCommands()) {
            continue;
        }

        const ModelBatch& batch = modelRepo.getModelBatch(ModelBatchKey{ MeshIndexType::USHORT, VertexType::STANDARD_MODEL, passType });

        // Rebind these as the dynamic model renderer may have replaced them
        // TODO: Have a better way to track this stuff
        VGBuffer vao = batch.getVao();
        GL.glVertexArrayVertexBuffer(vao, MODEL_TRANSFORMS_BINDING_POINT, modelManager.mTransformsVbo, 0, sizeof(f32m4));
        GL.glVertexArrayVertexBuffer(vao, MODEL_VARIANT_INDICES_BINDING_POINT, modelManager.mVariantsVbo, 0, sizeof(ui32));
        GL.glVertexArrayVertexBuffer(vao, MODEL_DAMAGE_INDICES_BINDING_POINT, modelManager.mDamageModelIndexVbo, 0, sizeof(ui32));

        batch.bindStaticModelAttribs();

        GL.glBindVertexArray(vao);
        assert(batch.getIndexType() == MeshIndexType::USHORT);
        drawCommands.multiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT);
    }
    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelShadows");
}
