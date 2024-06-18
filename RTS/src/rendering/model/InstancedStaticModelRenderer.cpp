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
    mShadowMapperShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("shadow_mapper_instd"));
    mSmudgeShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("smudge_model"));
    mWaterShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("water_model"));

    static_assert(e_count(MaterialRenderPassType) == 3);
}

InstancedStaticModelRenderer::~InstancedStaticModelRenderer() = default;

void InstancedStaticModelRenderer::setActiveWorld(World& world) {
    mWeatherManager = &world.getWeatherManager();
}

void InstancedStaticModelRenderer::renderModelPass(const ModelBatchMap& modelInstances, const Camera3D& camera, MaterialRenderPassType passType, const CubemapDef* skyCubeMap) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    PROFILE_FUNCTION();

    // TODO: Material specific, we lose 10fps disabling this
    glDisable(GL_CULL_FACE);
    const MaterialShaderDef* def = nullptr;

    switch (passType) {
        case MaterialRenderPassType::Default:
            def = mStandardShader;
            break;
        case MaterialRenderPassType::Smudge:
            def = mSmudgeShader;
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
    for (auto& [modelId, instanceData] : modelInstances) {
        for (int m = 0; m < instanceData.mMeshCount; ++m) {
            if (!instanceData.mDrawCommands[m]) {
                break;
            }
            const Mesh& mesh = *(instanceData.mMesh[m]);

            if (mesh.getRenderPass() == passType) {
                // Copy draw commands
                GLDrawCommandBuffer& drawCommands = *instanceData.mDrawCommands[m];
                if (!drawCommands.getNumActiveCommands()) {
                    continue;
                }

                // TODO: Not uniform, instead per model when we have improved batching
                if (windUniform) {
                    glUniform1i(windUniform, (GLint)mesh.getSubmeshData()->windType);
                }

                // Variant data
                assert(mesh.mVariantDataUbo);
                glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_DAMAGE_DATA_SSBO, instanceData.mDamageZonesSSBO);

                // Bind our transforms every frame as we could be using different instanced static model managers
                GL.glVertexArrayVertexBuffer(mesh.mGpuData.mVao, MODEL_TRANSFORMS_BINDING_POINT, instanceData.mTransformsVbo, 0, sizeof(f32m4));
                GL.glVertexArrayVertexBuffer(mesh.mGpuData.mVao, MODEL_VARIANT_INDICES_BINDING_POINT, instanceData.mVariantsVbo, 0, sizeof(ui8));
                GL.glVertexArrayVertexBuffer(mesh.mGpuData.mVao, MODEL_DAMAGE_INDICES_BINDING_POINT, instanceData.mDamageModelIndexVbo, 0, sizeof(ui32));

                // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
                //// Make sure we created a fence for this instance
                //assert(instanceData.mFenceSync);
                //// Make sure all compute commands are finished
                //while (true) {
                //    const GLenum res = glClientWaitSync(instanceData.mFenceSync, GL_SYNC_FLUSH_COMMANDS_BIT, 100);
                //    if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED) break;
                //}
                //glDeleteSync(instanceData.mFenceSync);
                //instanceData.mFenceSync = 0;

                //const ui32 totalCommands = *instanceData.mNumVisibleMeshesBufferPtr;
                //assert(totalCommands == drawCommands.mDrawCommands.size());

                //GL_INVALID_OPERATION is generated if no buffer is bound to the GL_ELEMENT_ARRAY_BUFFER binding, or if such a buffer's data store is currently mapped.
                //GL_INVALID_OPERATION is generated if a non - zero buffer object name is bound to an enabled array or to the GL_DRAW_INDIRECT_BUFFER binding and the buffer object's data store is currently mapped.
                //GL_INVALID_OPERATION is generated if a geometry shader is active and mode is incompatible with the input primitive type of the geometry shader in the currently installed program object.

                MeshDrawer::drawIndirect(mesh.mGpuData, &drawCommands);
                break;
            }
        }
    }
    
    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelPass");
}

void InstancedStaticModelRenderer::renderModelShadows(const ModelBatchMap& modelInstances, const ShadowPassShaderData& shaderData, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();

    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }
    // TODO: Material specific
    glDisable(GL_CULL_FACE);

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialShaderForRender(*mShadowMapperShader);
    glUniformMatrix4fv(mShadowMapperShader->getUniform("unShadowFrustumMatrices[0]"), MAX_SHADOW_CASCADE_LEVELS, false, &(*shaderData.shadowFrustumMatrices)[0][0]);

    for (auto& [modelId, instanceData] : modelInstances) {
        for (int m = 0; m < instanceData.mMeshCount; ++m) {
            if (!instanceData.mDrawCommandsShadows[m]) {
                continue;
            }

            // TODO: Have a no shadow render type?
            GLDrawCommandBuffer& drawCommands = *instanceData.mDrawCommandsShadows[m];
            if (!drawCommands.getNumActiveCommands()) {
                continue;
            }

            const Mesh& mesh = *instanceData.mMesh[m];
            MeshDrawer::drawIndirect(mesh.mGpuData, &drawCommands);
        }
    }
    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelShadows");
}
