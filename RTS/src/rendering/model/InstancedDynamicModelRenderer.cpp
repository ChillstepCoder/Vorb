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

void InstancedDynamicModelRenderer::prepareFrame(const std::vector<DynamicModelInstanceState>& dynamicModels, const Camera3D& camera) {
    
    if (sDebugOptions.mHideDynamicModels) {
        return;
    }
    
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    // Clean up last frame
    for (auto& [modelId, batch] : mModelBatchesThisFrame) {
        batch.mVisibleIndices.clear();
        for (auto& meshData : batch.mMeshData) {
            meshData.visibleCount = 0;
        }
    }

    for (int i = 0; i < e_count(MaterialRenderPassType); ++i) {
        mDrawCommandsThisFrame[i].clear();
    }

    // Cull instances and initialize batches
    size_t totalTransforms = 0;
    for (size_t i = 0; i < dynamicModels.size(); ++i) {
        const DynamicModelInstanceState& dynamicModel = dynamicModels[i];
        DynamicModelBatchData& batch = mModelBatchesThisFrame[dynamicModel.modelId];
        // Initialize if needed
        if (batch.mIsInitialized == false) [[unlikely]] {
            if (!batch.mModelHandle) {
                batch.mModelHandle = ModelRepository::get().getAssetHandle(dynamicModel.modelId);
            }
            if (const ModelDef* modelDef = batch.mModelHandle->tryGetLoadedAsset()) {
                batch.mIsInitialized = true;
                batch.mBoundingSphereRadius = modelDef->mBoundingSphereRadius;
                for (ui32 meshIndex = 0; meshIndex < modelDef->getNumMeshes(); ++meshIndex) {
                    DynamicModelBatchData::MeshData& newMeshData = batch.mMeshData.emplace_back();
                    newMeshData.mesh = &modelDef->getMesh(meshIndex);
                    // TODO Use render pass index
                    for (int i = 0; i < 4; ++i) {
                        newMeshData.drawInfos[i] = newMeshData.mesh->mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
                    }
                }
            }
            else {
                continue;
            }
        }

        // CPU Culling
        if (camera.sphereIsVisible(dynamicModel.position, batch.mBoundingSphereRadius)) {
            for (auto& meshData : batch.mMeshData) {
                ++meshData.visibleCount;
            }
            batch.mVisibleIndices.emplace_back(i);
            ++totalTransforms;
        }
    }

    if (totalTransforms == 0) {
        return;
    }

    // Allocate transforms buffer
    constexpr ui32 FUZZ = 20; // Helps account for fluctuating instance counts
    if (!mTransformsBuffer) {
        mTransformsBuffer = std::make_unique<GpuStreamingDataBuffer>(totalTransforms, sizeof(f32m4));
        mVariantsBuffer = std::make_unique<GpuStreamingDataBuffer>(totalTransforms, sizeof(ui8));
    }
    else if (totalTransforms > mTransformsBuffer->getMaxElements() ||
        totalTransforms < mTransformsBuffer->getMaxElements() * 0.5f - FUZZ) {
        // Grow or shrink if needed
        mTransformsBuffer = std::make_unique<GpuStreamingDataBuffer>(totalTransforms + FUZZ, sizeof(f32m4));
        mVariantsBuffer = std::make_unique<GpuStreamingDataBuffer>(totalTransforms + FUZZ, sizeof(ui8));
    }
    
    f32m4* transformsArray = static_cast<f32m4*>(mTransformsBuffer->frameBeginAndGetDataForUpdate());
    ui8* variantsArray = static_cast<ui8*>(mVariantsBuffer->frameBeginAndGetDataForUpdate());
    assert(transformsArray && variantsArray);

    // Process all batches
    GLuint transformOffset = mTransformsBuffer->getCurrentElementOffset();
    GLuint transformIndex = 0;
    for (auto&& it = mModelBatchesThisFrame.begin(); it != mModelBatchesThisFrame.end();) {
        auto& [modelId, batch] = *it;
        const ModelLodParams& lodParams = ModelRepository::get().getLodParams(modelId);
        if (batch.mVisibleIndices.size() > 0) {

            // Allocate draw commands
            for (auto& meshData : batch.mMeshData) {
                if (meshData.visibleCount > 0) {
                    if (!meshData.drawCommands) {
                        meshData.drawCommands = std::make_unique<GLDrawCommandBuffer>(meshData.visibleCount);
                    }
                    else if (meshData.visibleCount > meshData.drawCommands->getCapacity() ||
                        meshData.visibleCount < meshData.drawCommands->getCapacity() * 0.5f - FUZZ) {
                        // Grow or shrink if needed
                        meshData.drawCommands = std::make_unique<GLDrawCommandBuffer>(meshData.visibleCount);
                    }
                    // TODO: Just store the render pass intead of the whole mesh?
                    mDrawCommandsThisFrame[e_cast(meshData.mesh->getRenderPass())].emplace_back(meshData.drawCommands.get(), meshData.mesh);
                    meshData.drawCommands->setNumActiveCommands(0);
                    meshData.drawCommands->frameBegin();
                }
                else {
                    meshData.drawCommands.reset();
                }
            }

            // Set draw commands and transforms
            for (ui32 index : batch.mVisibleIndices) {
                const DynamicModelInstanceState& dynamicModel = dynamicModels[index];
                // Set transform for this instance
                if (camera.sphereIsVisible(dynamicModel.position, lodParams.boundingSphereRadius)) {
                    const f32 distance2 = glm::length2(dynamicModel.position - camera.getPosition());
                    if (distance2 < lodParams.lodDistancesSQ[3]) {
                        transformsArray[transformIndex] = MathUtil::createTransformMatrix(dynamicModel.position, dynamicModel.orientation);
                        variantsArray[transformIndex] = 0; //dynamicModel.variantIndex; // TODO: Variants
                        for (auto& meshData : batch.mMeshData) {
                            MeshLODDrawInfo* drawInfos = meshData.drawInfos;
                            DrawElementsIndirectCommand& cmd = meshData.drawCommands->appendCommand();
                            // build draw command
                            cmd.instanceCount_ = 1;
                            cmd.baseInstance_ = transformOffset + transformIndex;
                            cmd.baseVertex_ = 0;
                            MeshLODDrawInfo drawInfo;
                            if (distance2 < lodParams.lodDistancesSQ[0] || sDebugOptions.mDisableLOD) {
                                drawInfo = drawInfos[0];
                            }
                            else if (distance2 < lodParams.lodDistancesSQ[1]) {
                                drawInfo = drawInfos[1];
                            }
                            else if (distance2 < lodParams.lodDistancesSQ[2]) {
                                drawInfo = drawInfos[2];
                            }
                            else {
                                drawInfo = drawInfos[3];
                            }
                            cmd.count_ = drawInfo.indexCount;
                            cmd.firstIndex_ = drawInfo.startIndex;
                        } // meshData
                        ++transformIndex;
                    }
                }
            } // modelIndex

            // Upload draw commands
            for (auto& meshData : batch.mMeshData) {
                if (meshData.drawCommands) {
                    meshData.drawCommands->uploadDrawCommands();
                }
            }

            ++it;
        }
        else {
            it = mModelBatchesThisFrame.erase(it);
        }
    }
    // TODO: Had a crash here (0 == 273)
    assert(transformIndex == totalTransforms);
    mTransformsBuffer->flushDataAndIncrementFrame(totalTransforms);
    mVariantsBuffer->flushDataAndIncrementFrame(totalTransforms);
}

void InstancedDynamicModelRenderer::renderModelPass(MaterialRenderPassType renderPass) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideDynamicModels) {
        return;
    }
    
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }
    PROFILE_FUNCTION();

    MaterialRenderer::bindMaterialShaderForRender(*mStandardMaterial);
    const VGUniform windUniform = mStandardMaterial->getUniform("unWindType");

    for (auto& drawCommandPair : mDrawCommandsThisFrame[e_cast(renderPass)]) {
        GLDrawCommandBuffer* drawCommands = drawCommandPair.first;
        const Mesh& mesh = *drawCommandPair.second;
        // TODO: Do elsewhere
        mesh.bindStaticModelAttribs();


        // Variant data
        assert(mesh.mVariantDataUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);

        // TODO: I think this might be cheaper as an SSBO so we aren't binding to every mesh
        mTransformsBuffer->bindAsVertexArrayVertexBuffer(mesh.mGpuData.mVao, MODEL_TRANSFORMS_BINDING_POINT, 0, sizeof(f32m4));
        mVariantsBuffer->bindAsVertexArrayVertexBuffer(mesh.mGpuData.mVao, MODEL_VARIANTS_BINDING_POINT, 0, sizeof(ui8));

        glUniform1i(windUniform, (GLint)mesh.getSubmeshData()->windType);

        MeshDrawer::drawIndirect(mesh.mGpuData, drawCommands);
    }

    checkGlError("InstancedDynamicModelRenderer::renderModelPass");
}
