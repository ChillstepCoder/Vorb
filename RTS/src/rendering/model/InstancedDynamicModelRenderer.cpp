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
    mStandardShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("dynamic_model"));
    mSmudgeShader = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("smudge"));
}

InstancedDynamicModelRenderer::~InstancedDynamicModelRenderer() = default;

void InstancedDynamicModelRenderer::prepareFrame(const DynamicModelInstanceStateContainer& dynamicModels, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    PROFILE_FUNCTION();

    if (sDebugOptions.mHideDynamicModels) [[unlikely]] {
        return;
    }
    
    if (!mShaderAssets.areAllAssetsLoaded()) [[unlikely]] {
        return;
    }

    // Zero old refs
    for (auto& [modelId, modelDefRef] : mModelDefRefs) {
        modelDefRef.refCount = 0;
    }

    const std::vector<DynamicModelInstanceState>& dynamicModelsVec = dynamicModels.getVec();

    constexpr ui32 DRAW_COMMANDS_FUZZ = 128; // Helps account for fluctuating instance counts
    for (MaterialRenderPassType r : {MaterialRenderPassType::Default, MaterialRenderPassType::Smudge}) {
        const ui32 maxCount = dynamicModels.getSubmeshCount(r);
        std::unique_ptr<GLDrawCommandBuffer>& bufferPtr = mDrawCommands[e_cast(r)];
        GLDrawCommandBuffer::reallocateFuzzedIfNeeded(bufferPtr, maxCount, DRAW_COMMANDS_FUZZ);
    }
    static_assert(e_count(MaterialRenderPassType) == 3, "Update this code if you add more passes");

    // Allocate transforms buffer
    const ui32 maxTransforms = dynamicModelsVec.size();
    constexpr ui32 TRANSFORMS_FUZZ = 64; // Helps account for fluctuating instance counts
    GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(mTransformsBuffer, maxTransforms, sizeof(f32m4), TRANSFORMS_FUZZ);
    GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(mVariantIndexBuffer, maxTransforms, sizeof(ui32), TRANSFORMS_FUZZ);
   
    f32m4* transformsArray = static_cast<f32m4*>(mTransformsBuffer->frameBeginAndGetDataForUpdate());
    ui32* variantIndexArray = static_cast<ui32*>(mVariantIndexBuffer->frameBeginAndGetDataForUpdate());

    ModelRepository& modelRepo = ModelRepository::get();

    for (auto& commandBuffer : mDrawCommands) {
        if (commandBuffer) {
            commandBuffer->frameBegin();
        }
    }

    // Cull instances and initialize batches
    size_t totalTransforms = 0;
    for (size_t i = 0; i < dynamicModelsVec.size(); ++i) {
        const DynamicModelInstanceState& dynamicModel = dynamicModelsVec[i];

        // TODO: Could this maybe be handled at a higher level?
        ++mModelDefRefs[dynamicModel.modelId].refCount;
        const ModelLodParams& lodParams = modelRepo.getLodParams(dynamicModel.modelId);
        // CPU Culling
        if (camera.sphereIsVisible(dynamicModel.getPositionLowPrecision(), lodParams.boundingSphereRadius)) {

            const f32v3 cameraRelativePos = f32v3(f64v3(dynamicModel.positionXY.x, dynamicModel.positionXY.y, dynamicModel.positionZ) - f64v3(camera.getPosition()));
            const f32 distance2 = glm::length2(cameraRelativePos);
            if (distance2 < lodParams.lodDistancesSQ[3]) {
                transformsArray[totalTransforms] = MathUtil::createTransformMatrix(cameraRelativePos, dynamicModel.orientation, 1.0f);
                variantIndexArray[totalTransforms] = modelRepo.getVariantArrayIndexDataForModel(dynamicModel.modelId).offset;

                ModelBatchSubmeshDrawDataSpanKey key = modelRepo.getDrawDataSpanKeyForModel(dynamicModel.modelId);
                const ModelBatchSubmeshDrawData* drawDataArray = modelRepo.getSubmeshDrawDataArrayForModel(key);

                //variantsArray[totalTransforms] = 0; //dynamicModel.variantIndex; // TODO: Variants
                for (int submeshIndex = 0; submeshIndex < key.count; ++submeshIndex) {
                    const ModelBatchSubmeshDrawData& drawData = drawDataArray[submeshIndex];
                    DrawElementsIndirectCommand& cmd = mDrawCommands[e_cast(drawData.renderPass)]->appendCommand();
                    // build draw command
                    cmd.instanceCount_ = 1;
                    cmd.baseInstance_ = totalTransforms;
                    cmd.baseVertex_ = drawData.baseVertex;
                    MeshLODDrawInfo drawInfo;
                    if (distance2 < lodParams.lodDistancesSQ[0] || sDebugOptions.mDisableLOD) {
                        drawInfo = drawData.lodDrawInfo[0];
                    }
                    else if (distance2 < lodParams.lodDistancesSQ[1]) {
                        drawInfo = drawData.lodDrawInfo[1];
                    }
                    else if (distance2 < lodParams.lodDistancesSQ[2]) {
                        drawInfo = drawData.lodDrawInfo[2];
                    }
                    else {
                        drawInfo = drawData.lodDrawInfo[3];
                    }
                    cmd.count_ = drawInfo.indexCount;
                    cmd.firstIndex_ = drawInfo.startIndex;
                } // meshData
                ++totalTransforms;
            }
        }
    }

    if (totalTransforms == 0) {
        return;
    }

    for (auto& commandBuffer : mDrawCommands) {
        if (commandBuffer && commandBuffer->getNumActiveCommands()) {
            commandBuffer->uploadDrawCommands();
        }
    }

    mTransformsBuffer->flushDataAndIncrementFrame(totalTransforms);
    mVariantIndexBuffer->flushDataAndIncrementFrame(totalTransforms);

    // Clear stale refs
    for (auto it = mModelDefRefs.begin(); it != mModelDefRefs.end();) {
        if (it->second.refCount == 0) [[unlikely]] {
            it = mModelDefRefs.erase(it);
        }
        else {
            ++it;
        }
    }
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

    const MaterialShaderDef* shaderDef = nullptr;
    switch (renderPass) {
        case MaterialRenderPassType::Default:
            shaderDef = mStandardShader;
            break;
        case MaterialRenderPassType::Smudge:
            shaderDef = mSmudgeShader;
            break;
        default:
            panic("Unsupported dynamic render pass {}", (int)renderPass);
            break;

    }

    MaterialRenderer::bindMaterialShaderForRender(*shaderDef);

    GLDrawCommandBuffer* drawCommands = mDrawCommands[e_cast(renderPass)].get();
    if (drawCommands && drawCommands->getNumActiveCommands()) {

        ModelRepository& modelRepo = ModelRepository::get();
        // Talia said we never need more than 65536 verts
        // TODO: Handle skeletal vertex type? Or better yet skeletal attributes are separated?
        const ModelBatch& batch = modelRepo.getModelBatch(ModelBatchKey{ MeshIndexType::USHORT, VertexType::STANDARD_MODEL, renderPass });

        // Variant data
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_SSBO, modelRepo.getModelVariantDataSSBO());

        // Bind our transforms every frame as we could be using different instanced static model managers
        VGBuffer vao = batch.getVao();
        mTransformsBuffer->bindAsVertexArrayVertexBuffer(vao, MODEL_TRANSFORMS_BINDING_POINT, 0, sizeof(f32m4));
        mVariantIndexBuffer->bindAsVertexArrayVertexBuffer(vao, MODEL_INSTANCE_DATA_BINDING_POINT, 0, sizeof(ui32));

        batch.bindStaticModelAttribs();
        batch.setInstanceDataAttribFormat(1);

        glBindVertexArray(vao);
        assert(batch.getIndexType() == MeshIndexType::USHORT);
        drawCommands->multiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT);

        checkGlError("InstancedDynamicModelRenderer::renderModelPass");
    }
}
