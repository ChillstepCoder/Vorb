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
    mQuadVBO.init();
}

MaterialRenderer::~MaterialRenderer()
{

}

void MaterialRenderer::renderFullScreenQuad(const Material& material) const {

    bindMaterialForRender(material, nullptr);

    mQuadVBO.draw();
}

void MaterialRenderer::renderQuadMesh(const QuadMesh& quadMesh, const Material& material, const vg::DepthState& depthState/* = vg::DepthState::FULL*/) const
{
    // TODO: Here we are rebinding to make sure nobody fucked with our textures, but would be nice to avoid re-binds when iterating chunks?
    bindMaterialForRender(material, nullptr);

    quadMesh.draw(material.mProgram, depthState);
}

void MaterialRenderer::renderMaterialToQuadWithTexture(const Material& material, VGTexture texture, const f32v4& worldSpaceRect)
{
    assert(texture);
    ui32 textureIndex;
    bindMaterialForRender(material, &textureIndex);

    // TODO: Uniform buffer object for static uniforms
    VGUniform textureUniform = material.mProgram.getUniform("Texture");
    glActiveTexture(textureIndex);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(textureUniform, textureIndex);

    // TODO: Cache so we dont need a string lookup? (I think it just has to be cached on outer loop)
    VGUniform rectUniform = material.mProgram.getUniform("Rect");
    glUniform4fv(rectUniform, 1, &(worldSpaceRect.x));

    mQuadVBO.draw();
}

void MaterialRenderer::bindMaterialForRender(const Material& material, OUT ui32* nextAvailableTextureIndex /* =nullptr */) const {
    material.use();
    uploadUniforms(material, nextAvailableTextureIndex);
}

// TODO: Batch upload uniforms so we dont do it multiple times redundantly
void MaterialRenderer::uploadUniforms(const Material& material, OUT ui32* nextAvailableTextureIndex) const {
    //  TODO: We are redundant with the texture uniform uploads. Can uniform buffer object save us here?
    // https://www.khronos.org/opengl/wiki/Uniform_Buffer_Object
    ui32 availableTextureIndex = 0;
    const GlobalRenderData& renderData = mRenderContext.getRenderData();
    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialUniform::Atlas:
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D_ARRAY, renderData.atlas);
                // TODO: Stop re-setting this
                vg::SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D_ARRAY);
                break;
            case MaterialUniform::Time:
                glUniform1f(it.second, (float)sTotalTimeSeconds);
                break;
            case MaterialUniform::TimeOfDay:
                glUniform1f(it.second, renderData.timeOfDay);
                break;
            case MaterialUniform::SunColor:
                glUniform3f(it.second, renderData.sunColor.x, renderData.sunColor.y, renderData.sunColor.z);
                break;
            case MaterialUniform::SunHeight:
                glUniform1f(it.second, renderData.sunHeight);
                break;
            case MaterialUniform::SunPosition:
                glUniform1f(it.second, renderData.sunPosition);
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
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getGeometryTexture(FBO_GEOMETRY_COLOR));
                break;
            case MaterialUniform::FboLight:
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getLightTexture());
                break;
            case MaterialUniform::FboDepth:
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getDepthTexture());
                break;
            case MaterialUniform::FboNormals:
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getGeometryTexture(FBO_GEOMETRY_NORMAL));
                break;
            case MaterialUniform::PrevFbo0:
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getPrevGBuffer().getGeometryTexture(FBO_GEOMETRY_COLOR));
                break;
            case MaterialUniform::PrevFboDepth:
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
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
            case MaterialUniform::FboShadowHeight:
                glActiveTexture(GL_TEXTURE0 + availableTextureIndex);
                glUniform1i(it.second, availableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getShadowGBuffer().getGeometryTexture(0));
                break;
        }
        static_assert((int)MaterialUniform::COUNT == 19, "Update for new uniform type");
    }
    if (nextAvailableTextureIndex) {
        *nextAvailableTextureIndex = availableTextureIndex;
    }
}
