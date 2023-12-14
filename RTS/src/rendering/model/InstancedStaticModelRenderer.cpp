#include "stdafx.h"
#include "InstancedStaticModelRenderer.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshDrawer.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/post_process/ShadowDetail.h"
#include "rendering/RenderContext.h"
#include "rendering/post_process/ShadowPassShaderData.h"
#include "options/DebugOptions.h"

#include "world/World.h"
#include "weather/WeatherManager.h"

#include "camera/Camera3D.h"

#include "rendering/gl/GL.h"


InstancedStaticModelRenderer::InstancedStaticModelRenderer() {

    mStandardShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("standard_model"));
    mShadowMapperShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("shadow_mapper_instd"));
    mSmudgeShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("smudge_model"));
}

InstancedStaticModelRenderer::~InstancedStaticModelRenderer() = default;

void InstancedStaticModelRenderer::onWorldBegin(World& world) {
    mWeatherManager = &world.getWeatherManager();
}

void InstancedStaticModelRenderer::renderModelPass(const ModelInstanceMap& modelInstances, const Camera3D& camera, MaterialRenderPassType passType) {
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
        default:
            assert(false);
            break;

    }
    static_assert(e_count(MaterialRenderPassType) == 2);

    MaterialRenderer::bindMaterialShaderForRender(*def);
    const VGUniform windUniform = def->getUniform("unWindType");
    glUniform1f(def->getUniform("unSnowLevel"), mWeatherManager->mSnowLevel);
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

                glUniform1i(windUniform, (GLint)mesh.getSubmeshData()->windType);

                // Bind our transforms every frame as we could be using different instanced static model managers
                GL.glVertexArrayVertexBuffer(mesh.mGpuData.mVao, MODEL_TRANSFORMS_BINDING_POINT, instanceData.mTransformsVbo, 0, sizeof(f32m4));

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
                MeshDrawer::drawIndirect(mesh.mGpuData, &drawCommands);
                break;
            }
        }
    }
    
    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelPass");
}

void InstancedStaticModelRenderer::renderModelShadows(const ModelInstanceMap& modelInstances, const ShadowPassShaderData& shaderData, const Camera3D& camera) {
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
