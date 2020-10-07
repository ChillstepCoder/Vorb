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
    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialUniform::Atlas:
                glActiveTexture(GL_TEXTURE0);
                glUniform1i(it.second, 0);
                glBindTexture(GL_TEXTURE_2D_ARRAY, mRenderContext.getRenderData().atlas);
                vg::SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D_ARRAY);
                break;
            case MaterialUniform::Time:
                glUniform1f(it.second, (float)sTotalTimeSeconds);
                break;
            case MaterialUniform::WMatrix: {
                f32m4 world(1.0f);
                glUniformMatrix4fv(it.second, 1, false, &world[0][0]);
                break;
            }
            case MaterialUniform::WVPMatrix:
                assert(false); //Not implemented
                break;
            case MaterialUniform::VPMatrix:
                glUniformMatrix4fv(it.second, 1, false, &mRenderContext.getRenderData().mainCamera->getCameraMatrix()[0][0]);
                break;
        }
        static_assert((int)MaterialUniform::COUNT == 6, "Update for new uniform type");
    }
}
