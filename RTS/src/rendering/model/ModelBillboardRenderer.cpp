#include "stdafx.h"
#include "ModelBillboardRenderer.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include "rendering/model/ModelBillboardLodManager.h"

#include "options/DebugOptions.h"

#include <Vorb/graphics/FullscreenTriangleVAO.h>

ModelBillboardRenderer::ModelBillboardRenderer() {
    mShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("model_billboard"));
}

void ModelBillboardRenderer::renderBillboards(const ModelBillboardLodManager& billboardManager) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels) [[unlikely]] {
        return;
    }

    if (!mShader->isLoaded()) [[unlikely]] {
        return;
    }

    const MaterialShaderDef& shader = mShader->getLoadedOrUnloadedAsset();
    ui32 nextTextureIndex = 0;
    MaterialRenderer::bindMaterialShaderForRender(shader, &nextTextureIndex);

    // TODO: Snow
    // glUniform1f(def->getUniform("unSnowLevel"), mWeatherManager->mSnowLevel);

    const GpuStreamingDataBuffer* billboardBuffer = billboardManager.getBillboardDataBuffer();
    if (!billboardBuffer) [[unlikely]] {
        return;
    }

    const ui32 offset = billboardBuffer->getElementOffsetLastFlush();
    glUniform1ui(shader.getUniform("unBaseInstanceOffset"), offset);
    billboardBuffer->bindBufferAsSSBO(BUFFER_BASE_MODEL_BILLBOARD_DATA_SSBO);

    // Render two triangles per billboard with no vertex data
    sGlobalFullTriangleVAO.drawNTriangles(billboardManager.getNumBillboards() * 2);

}
