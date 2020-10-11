#include "stdafx.h"
#include "MaterialRenderer.h"

#include "Material.h"
#include "rendering/QuadMesh.h"
#include "rendering/RenderContext.h"
#include "Camera2D.h"

#include <Vorb/graphics/SamplerState.h>

MaterialRenderer::MaterialRenderer(const RenderContext& renderContext) :
    mRenderContext(renderContext)
{
    mScreenVBO.init();
}

MaterialRenderer::~MaterialRenderer()
{

}

void MaterialRenderer::renderMaterialToScreen(const Material& material) {
    material.use();

    // Enable depth test unless disabled by depth input
    glDepthMask(true);

    uploadUniforms(material);

    mScreenVBO.draw();
}

void MaterialRenderer::renderQuadMesh(const QuadMesh& quadMesh, const Material& material)
{
    material.use();

    uploadUniforms(material);

    quadMesh.draw(material.mProgram);
}

void MaterialRenderer::uploadUniforms(const Material& material) {
    ui32 mAvailableTextureIndex = 0;
    const GlobalRenderData& renderData = mRenderContext.getRenderData();
    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialUniform::Atlas:
                glActiveTexture(GL_TEXTURE0 + mAvailableTextureIndex);
                glUniform1i(it.second, mAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D_ARRAY, renderData.atlas);
                // TODO: Stop re-setting this
                vg::SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D_ARRAY);
                break;
            case MaterialUniform::Time:
                glUniform1f(it.second, (float)sTotalTimeSeconds);
                break;
            case MaterialUniform::WMatrix: {
                // TODO: Get rid or replace no op
                f32m4 world(1.0f);
                glUniformMatrix4fv(it.second, 1, false, &world[0][0]);
                break;
            }
            case MaterialUniform::WVPMatrix:
                assert(false); //Not implemented
                break;
            case MaterialUniform::VPMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.mainCamera->getCameraMatrix()[0][0]);
                break;
            case MaterialUniform::Fbo0:
                glActiveTexture(GL_TEXTURE0 + mAvailableTextureIndex);
                glUniform1i(it.second, mAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getGeometryTexture(0));
                break;
            case MaterialUniform::FboDepth:
                glActiveTexture(GL_TEXTURE0 + mAvailableTextureIndex);
                glUniform1i(it.second, mAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getDepthTexture());
                break;
            case MaterialUniform::PrevFbo0:
                glActiveTexture(GL_TEXTURE0 + mAvailableTextureIndex);
                glUniform1i(it.second, mAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getPrevGBuffer().getGeometryTexture(0));
                break;
            case MaterialUniform::PrevFboDepth:
                glActiveTexture(GL_TEXTURE0 + mAvailableTextureIndex);
                glUniform1i(it.second, mAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getPrevGBuffer().getDepthTexture());
                break;
            case MaterialUniform::PixelDims: {
                const f32v2 pixelDims = 1.0f / mRenderContext.getCurrentFramebufferDims();
                glUniform2f(it.second, pixelDims.x, pixelDims.y);
                break;
            }
            case MaterialUniform::ZoomScale:
                glUniform1f(it.second, renderData.mainCamera->getScale());
                break;
        }
        static_assert((int)MaterialUniform::COUNT == 12, "Update for new uniform type");
    }
}
