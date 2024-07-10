#include "stdafx.h"
#include "ModelHighlightRenderer.h"

#include "camera/Camera3D.h"

#include "options/DebugOptions.h"

#include "interact/SelectedObjectData.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/mesh/MeshDrawer.h"
#include "rendering/StencilBufferIDs.h"

#include "resources/ModelRepository.h"

#include "definitions/ModelDef.h"

#include "util/MathUtil.hpp"

#include <Vorb/graphics/DepthState.h>

ModelHighlightRenderer::ModelHighlightRenderer() {
    AssetHandlePtr<MaterialShaderDef> highlightShaderHandle = MaterialShaderRepository::get().getAssetHandle(CStrToken("model_highlight"));
    mHighlightShader = &highlightShaderHandle->getLoadedOrUnloadedAsset();
    mHighlightShaderHandle = std::move(highlightShaderHandle);
}

void ModelHighlightRenderer::renderModelHighlight(const SelectedObjectData& selectedObject, const Camera3D& camera) {
    if (selectedObject.modelId == INVALID_MODEL_ID) {
        return;
    }
    if (!mHighlightShaderHandle->isLoaded()) {
        return;
    }

    AssetHandlePtr<ModelDef> mModelHandle = ModelRepository::get().getAssetHandle(selectedObject.modelId);
    const ModelDef* def = mModelHandle->tryGetLoadedAsset();
    if (!def) {
        return;
    }

    MaterialRenderer::bindMaterialShaderForRender(*mHighlightShader);

    const f32m4 modelMatrix = MathUtil::createTransformMatrix(selectedObject.position, selectedObject.orientation, selectedObject.scale);

    // TODO: Variant
    const f32v4 colorf = sDebugOptions.mObjectHighlightColor.toVec4();
    glUniform4fv(mHighlightShader->getUniform("unColor"), 1, &(colorf.x));
    glUniformMatrix4fv(mHighlightShader->getUniform("unModelMatrix"), 1, GL_FALSE, &modelMatrix[0][0]);

    vg::DepthState::NONE.set();

    // Reverse culling gives more pleasant results
    glCullFace(GL_FRONT);
    glEnable(GL_STENCIL_TEST);

    for (int i = 0; i < 2; ++i) {
        if (i == 0) {
            // Render the inner core
            glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::HIGHLIGHT_CORE), 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glUniform1f(mHighlightShader->getUniform("unSize"), 0.0f);
            glUniform1f(mHighlightShader->getUniform("unAlphaThreshold"), 0.0f);
            glColorMask(false, false, false, false);
        }
        else {
            glStencilFunc(GL_NOTEQUAL, e_cast(StencilBufferIDs::HIGHLIGHT_CORE), 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            glUniform1f(mHighlightShader->getUniform("unSize"), sDebugOptions.mObjectHighlightSize);
            glUniform1f(mHighlightShader->getUniform("unAlphaThreshold"), sDebugOptions.mObjectHighlightDitherThreshold);
            glColorMask(true, true, true, true);
        }

        for (i32 m = 0; m < def->getNumMeshes(); ++m) {
            const Mesh& mesh = def->getMesh(m);
            mesh.bindDynamicModelAttribs();
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
            MeshDrawer::draw(mesh.mGpuData, MeshLODLevel::Highest);
        }

    }

    vg::DepthState::restorePrevious();

    glCullFace(GL_BACK);

    glDisable(GL_STENCIL_TEST);
}
