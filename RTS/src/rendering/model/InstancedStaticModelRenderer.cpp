#include "stdafx.h"
#include "InstancedStaticModelRenderer.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/post_process/ShadowLodDetail.h"
#include "rendering/RenderContext.h"
#include "rendering/post_process/ShadowPassShaderData.h"
#include "options/DebugOptions.h"

#include "camera/Camera3D.h"

#include "rendering/gl/GL.h"


InstancedStaticModelRenderer::InstancedStaticModelRenderer() {
    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mStandardMaterial = materialManager.getMaterialShader("standard_model");
    mShadowMapperMaterial = materialManager.getMaterialShader("shadow_mapper_instanced");
    mSmudgeShader = materialManager.getMaterialShader("smudge");
}

InstancedStaticModelRenderer::~InstancedStaticModelRenderer() {
  
}

void InstancedStaticModelRenderer::renderModelPass(const ModelInstanceMap& modelInstances, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;

    PROFILE_FUNCTION();

    // TODO: Material specific, we lose 10fps disabling this
    glDisable(GL_CULL_FACE);

    MaterialRenderer::bindMaterialForRender(*mStandardMaterial);
    for (auto& it : modelInstances) {
        const StaticModelInstanceData& instanceData = it.second;
        if (!instanceData.mDrawCommands) {
            continue;
        }

        // Copy draw commands
        GLIndirectBuffer& drawCommands = *instanceData.mDrawCommands;
        const size_t drawCommandsSize = drawCommands.mDrawCommands.size();
        if (!drawCommandsSize) {
            continue;
        }

        ModelID modelId = it.first;
        const Model3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).mModel;
        const Mesh& mesh = *model.getMesh();

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
        assert(instanceData.mInstanceTransforms.size() <= drawCommandsSize);
        mesh.drawIndirect(instanceData.mInstanceTransforms.size(), &drawCommands);
    }
    
    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelPass");
}

void InstancedStaticModelRenderer::renderModelShadows(const ModelInstanceMap* allModelPasses, const ShadowPassShaderData& shaderData, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    // TODO: Material specific
    glDisable(GL_CULL_FACE);

    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialForRender(*mShadowMapperMaterial);
    glUniformMatrix4fv(mShadowMapperMaterial->getUniform("unShadowFrustumMatrices[0]"), MAX_SHADOW_CASCADE_LEVELS, false, &(*shaderData.shadowFrustumMatrices)[0][0]);
    for (int ri = 0; ri < e_cast(MaterialRenderPassType::COUNT); ++ri) {
        for (auto& it : allModelPasses[ri]) {
            // TODO: Have a no shadow render type?

            const StaticModelInstanceData& instanceData = it.second;
            if (!instanceData.mShadowDrawCommandsCount) {
                continue;
            }

            // Copy draw commands
            GLIndirectBuffer& drawCommands = *instanceData.mDrawCommandsShadows;
            const size_t drawCommandsSize = drawCommands.mDrawCommands.size();

            ModelID modelId = it.first;
            const Model3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).mModel;
            const Mesh& mesh = *model.getMesh();

            assert(instanceData.mShadowDrawCommandsCount <= drawCommandsSize);
            mesh.drawIndirect(instanceData.mShadowDrawCommandsCount, &drawCommands);
        }
    }

    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelShadows");
}
