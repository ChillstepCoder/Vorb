#include "stdafx.h"
#include "InstancedDynamicModelRenderer.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderDef.h"

#include "resources/ModelRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshDrawer.h"

#include "options/DebugOptions.h"

InstancedDynamicModelRenderer::InstancedDynamicModelRenderer() {
    mStandardMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("standard_model"));
    mSmudgeShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("smudge"));
}

InstancedDynamicModelRenderer::~InstancedDynamicModelRenderer() = default;

void InstancedDynamicModelRenderer::renderModelPass(const DynamicModelInstanceMap& modelInstances, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    PROFILE_FUNCTION();

    // TODO: Material specific, we lose 10fps disabling this
    glDisable(GL_CULL_FACE);

    MaterialRenderer::bindMaterialShaderForRender(*mStandardMaterial);
    const VGUniform windUniform = mStandardMaterial->getUniform("unWindType");
    for (auto& it : modelInstances) {
        const DynamicMeshInstanceData& instanceData = it.second;
        if (!instanceData.mDrawCommands) {
            continue;
        }

        // Copy draw commands
        GLDrawCommandBuffer& drawCommands = *instanceData.mDrawCommands;
        const size_t drawCommandsSize = drawCommands.getDrawCommands().size();
        if (!drawCommandsSize) {
            continue;
        }

        ModelID modelId = it.first;
        const Mesh& mesh = *instanceData.mMesh;

        // TODO: Do elsewhere
        mesh.bindModelTransformAttribs();

        mTransformsBuffer->bindAsVertexArrayVertexBuffer(mesh.mGpuData.mVao, MODEL_TRANSFORMS_BINDING_POINT, 0, sizeof(f32m4));

        glUniform1i(windUniform, (GLint)mesh.getSubmeshData()->windType);

        MeshDrawer::drawIndirect(mesh.mGpuData, &drawCommands);
    }

    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelPass");
}
