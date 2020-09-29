#include "stdafx.h"
#include "MaterialRenderer.h"

#include "Material.h"
#include "rendering/RenderContext.h"

MaterialRenderer::MaterialRenderer()
{
    mScreenVBO.init();
}

MaterialRenderer::~MaterialRenderer()
{

}

void MaterialRenderer::renderMaterialToScreen(const Material& material, const RenderContext& renderContext) {
    material.use();

    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialUniform::Atlas:
                // uhhhh
                break;
            case MaterialUniform::Time:
                glUniform1f(it.second, (float)sTotalTimeSeconds);
                break;
            case MaterialUniform::WMatrix: {
                f32m4 world(1.0f);
                // TODO: cache uniform id?
                glUniformMatrix4fv(it.second, 1, false, &world[0][0]);
                break;
            }
            case MaterialUniform::WVPMatrix:
                assert(false); //Not implemented
                break;
            case MaterialUniform::VPMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderContext.GetRenderData().viewProjectionMatrix[0][0]);
                break;
        }
        static_assert((int)MaterialUniform::COUNT == 6, "Update for new uniform type");
    }

    mScreenVBO.draw();
}
