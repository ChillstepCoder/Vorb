#include "stdafx.h"
#include "InstancedDynamicModelRenderer.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderDef.h"

#include "resources/ModelRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshDrawer.h"

#include "options/DebugOptions.h"

#include "camera/Camera3D.h"

InstancedDynamicModelRenderer::InstancedDynamicModelRenderer() {
    mStandardMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("standard_model"));
    mSmudgeShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("smudge"));
}

InstancedDynamicModelRenderer::~InstancedDynamicModelRenderer() = default;

void InstancedDynamicModelRenderer::renderModelPass(const std::vector<DynamicModelRenderState>& dynamicModels, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    PROFILE_FUNCTION();

    // TODO: Material specific, we lose 10fps disabling this
    glDisable(GL_CULL_FACE);

    // Clean up last frame
    for (auto&& it : mModelInstancesThisFrame) {
        it.second.mVisibleIndices.clear();
    }

    for (size_t i = 0; i < dynamicModels.size(); ++i) {
        const DynamicModelRenderState& dynamicModel = dynamicModels[i];
        DynamicModelInstanceData& data = mModelInstancesThisFrame[dynamicModel.modelId];
        if (data.mIsInitialized == false) [[unlikely]] {
            if (const ModelDef* modelDef = data.mModelHandle.tryGetLoadedAsset()) {
                data.mIsInitialized = true;
                f32m4 transform = MathUtil::createTransformMatrix(dynamicModel.position, dynamicModel.orientation);
                xx; // TODO: USE TRANSFORM
                for (ui32 meshIndex = 0; meshIndex < modelDef->getNumMeshes(); ++meshIndex) {
                   DynamicModelInstanceData::MeshData& newMeshData = data.mMeshData.emplace_back();
                   newMeshData.mesh = &modelDef->getMesh(meshIndex);
                   // TODO Use render pass index
                   for (int i = 0; i < 4; ++i) {
                       newMeshData.drawInfos[i] = newMeshData.mesh->mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
                   }
                   newMeshData.mesh->getRenderPass();
                   ax;
                }
            }
            else {
                continue;
            }
        }

        

       
            if (camera.sphereIsVisible(dynamicModel.position, modelDef->mBoundingSphereRadius)) {
                data.mVisibleIndices.emplace_back(i);
            }
        }
    }
    // Allocate needed data
    GLuint transformIndex = 0;
    for (auto&& it = mModelInstancesThisFrame.begin(); it != mModelInstancesThisFrame.end();) {
        DynamicModelInstanceData& data = it->second;
        const size_t visibleCount = data.mVisibleIndices.size();
        if (visibleCount) {
            constexpr size_t BUFFER_FUZZ = 20;
            if (!data.mDrawCommands) {
                data.mDrawCommands = std::make_unique<GLDrawCommandBuffer>(visibleCount + BUFFER_FUZZ);
            }
            else {
                if (visibleCount > data.mDrawCommands->getCapacity() ||
                    visibleCount < (data.mDrawCommands->getCapacity() * 0.5f - BUFFER_FUZZ)) {
                    data.mDrawCommands = std::make_unique<GLDrawCommandBuffer>(visibleCount + BUFFER_FUZZ);
                }
            }
            // build draw commands
            auto commands = data.mDrawCommands->getDrawCommands();
            data.mDrawCommands->setNumActiveCommands(data.mVisibleIndices.size());
            for (int i = 0; i < data.mVisibleIndices.size(); ++i) {
                const DynamicModelRenderState& dynamicModel = dynamicModels[data.mVisibleIndices[i]];
                DrawElementsIndirectCommand& cmd = commands[i];
                cmd.instanceCount_ = 1;
                cmd.baseInstance_ = transformIndex;
                cmd.baseVertex_ = 0;
                MeshLODDrawInfo drawInfo, drawInfoShadow;
                f32 distance2 = glm::length2(dynamicModel.position - camera.getPosition());
                if ((distance2 < SQ(sDebugOptions.mLodDistances[0])) || sDebugOptions.mDisableLOD) {
                    drawInfo = drawInfos[0];
                    drawInfoShadow = drawInfos[1];
                }
                else if (distance2 < SQ(sDebugOptions.mLodDistances[1])) {
                    drawInfo = drawInfos[1];
                    drawInfoShadow = drawInfos[2];
                }
                else if (distance2 < SQ(sDebugOptions.mLodDistances[2])) {
                    drawInfo = drawInfos[2];
                    drawInfoShadow = drawInfos[3];
                }
                else {
                    drawInfo = drawInfos[3];
                    drawInfoShadow = drawInfos[3];
                }
                cmd.count_ = drawInfo.indexCount;
                cmd.firstIndex_ = drawInfo.startIndex;
                // TODO transform
                ++transformIndex;
            }
            ++it;
        }
        else {
            it = mModelInstancesThisFrame.erase(it);
        }
    }

    MaterialRenderer::bindMaterialShaderForRender(*mStandardMaterial);
    const VGUniform windUniform = mStandardMaterial->getUniform("unWindType");
    for (auto& it : modelInstances) {
        const ModelID modelId = it.first;
        const DynamicModelInstanceData& instanceData = it.second;
        const Mesh& mesh = *instanceData.mMesh;

        // CPU Culling
        int activeCount = 0;
        for (size_t i = 0; i < instanceData.mInstanceTransforms.size(); ++i) {
            DrawElementsIndirectCommand& cmd = inDrawCommands.getDrawCommands().data()[activeCount];
            const f32m4& transform = instanceData.mInstanceTransforms[i];
            // Columns are first
            const f32v3& pos = reinterpret_cast<const f32v3&>(transform[3]);
            if (camera.sphereIsVisible(pos, 10.0f)) {
                cmd.instanceCount_ = 1;
                cmd.baseInstance_ = i;
                cmd.baseVertex_ = 0;
                MeshLODDrawInfo drawInfo, drawInfoShadow;
                f32 distance2 = glm::length2(pos - camera.getPosition());
                if ((distance2 < SQ(sDebugOptions.mLodDistances[0])) || sDebugOptions.mDisableLOD) {
                    drawInfo = drawInfos[0];
                    drawInfoShadow = drawInfos[1];
                }
                else if (distance2 < SQ(sDebugOptions.mLodDistances[1])) {
                    drawInfo = drawInfos[1];
                    drawInfoShadow = drawInfos[2];
                }
                else if (distance2 < SQ(sDebugOptions.mLodDistances[2])) {
                    drawInfo = drawInfos[2];
                    drawInfoShadow = drawInfos[3];
                }
                else {
                    drawInfo = drawInfos[3];
                    drawInfoShadow = drawInfos[3];
                }
                cmd.count_ = drawInfo.indexCount;
                cmd.firstIndex_ = drawInfo.startIndex;
                ++activeCount;
            }
        }




        // Render
        if (!instanceData.mDrawCommands) {
            continue;
        }

        // Copy draw commands
        GLDrawCommandBuffer& drawCommands = *instanceData.mDrawCommands;
        const size_t drawCommandsSize = drawCommands.getDrawCommands().size();
        if (!drawCommandsSize) {
            continue;
        }

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
