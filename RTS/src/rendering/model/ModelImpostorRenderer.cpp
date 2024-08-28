#include "stdafx.h"
#include "ModelImpostorRenderer.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include "rendering/model/ModelImpostorManager.h"

#include "options/DebugOptions.h"

#include <Vorb/graphics/FullscreenTriangleVAO.h>

ModelImpostorRenderer::ModelImpostorRenderer() {
    mShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("model_billboard"));
}

void ModelImpostorRenderer::renderBillboards(const ModelImpostorManager& billboardManager) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mDisableImpostors) [[unlikely]] {
        return;
    }

    if (!mShader->isLoaded()) [[unlikely]] {
        return;
    }

    const MaterialShaderDef& shader = mShader->getLoadedOrUnloadedAsset();
    ui32 nextTextureIndex = 0;
    MaterialRenderer::bindMaterialShaderForRender(shader, &nextTextureIndex);

    billboardManager.bindMaterialBuffer();

    // TODO: Snow
    // glUniform1f(def->getUniform("unSnowLevel"), mWeatherManager->mSnowLevel);

    const GpuStreamingDataBuffer* billboardBuffer = billboardManager.getBillboardDataBuffer();
    if (!billboardBuffer) [[unlikely]] {
        return;
    }

    const ui32 offset = billboardBuffer->getElementOffsetLastFlush();
    glUniform1ui(shader.getUniform("unBaseInstanceOffset"), offset);
    billboardBuffer->bindBufferAsSSBO(BUFFER_BASE_MODEL_IMPOSTOR_DATA_SSBO);

    // Render two triangles per billboard with no vertex data
    sGlobalFullTriangleVAO.drawNTriangles(billboardManager.getNumBillboards() * 2);

}
