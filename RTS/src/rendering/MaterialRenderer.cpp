#include "stdafx.h"
#include "MaterialRenderer.h"

#include "Material.h"
#include "rendering/QuadMesh.h"
#include "rendering/RenderContext.h"
#include "camera/ICamera.h"

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "options/DebugOptions.h"

MaterialRenderer::MaterialRenderer(const RenderContext& renderContext) :
    mRenderContext(renderContext)
{
}

MaterialRenderer::~MaterialRenderer()
{

}

void MaterialRenderer::renderFullScreenQuad(const Material& material) const {

    bindMaterialForRender(material, nullptr);

    sGlobalFullQuadVBO.draw();
}

void MaterialRenderer::renderMesh(const MeshBase& mesh, const Material& material) const
{
    // TODO: Here we are rebinding to make sure nobody fucked with our textures, but would be nice to avoid re-binds when iterating chunks?
    bindMaterialForRender(material, nullptr);

    mesh.draw(material.mProgram);
}

void MaterialRenderer::renderMaterialToQuadWithTexture(const Material& material, VGTexture texture, const f32v4& worldSpaceRect)
{
    assert(texture);
    ui32 textureIndex;
    bindMaterialForRender(material, &textureIndex);

    // TODO: Uniform buffer object for static uniforms
    VGUniform textureUniform = material.mProgram.getUniform("Texture");
    glActiveTexture(GL_TEXTURE0 + textureIndex);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(textureUniform, textureIndex);

    // TODO: Cache so we dont need a string lookup? (I think it just has to be cached on outer loop)
    VGUniform rectUniform = material.mProgram.getUniform("Rect");
    glUniform4fv(rectUniform, 1, &(worldSpaceRect.x));

    sGlobalFullQuadVBO.draw();
}

void MaterialRenderer::renderMaterialToQuadWithTextureBindless(const Material& material, VGTexture texture, ui32 textureIndex, const f32v4& worldSpaceRect) {
    assert(texture);
    // TODO: Uniform buffer object for static uniforms
    VGUniform textureUniform = material.mProgram.getUniform("Texture");
    glActiveTexture(GL_TEXTURE0 + textureIndex);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(textureUniform, textureIndex);

    // TODO: Cache so we dont need a string lookup? (I think it just has to be cached on outer loop)
    VGUniform rectUniform = material.mProgram.getUniform("Rect");
    glUniform4fv(rectUniform, 1, &(worldSpaceRect.x));

    sGlobalFullQuadVBO.draw();
}

void MaterialRenderer::bindMaterialForRender(const Material& material, OUT ui32* nextAvailableTextureIndex /* =nullptr */) const {
    ui32 availableTextureIndex = 0;
    material.use(availableTextureIndex);
    uploadUniforms(material, availableTextureIndex);
    if (nextAvailableTextureIndex) {
        *nextAvailableTextureIndex = availableTextureIndex;
    }
}

// TODO: Batch upload uniforms so we dont do it multiple times redundantly
void MaterialRenderer::uploadUniforms(const Material& material, OUT ui32& nextAvailableTextureIndex) const {
    const GlobalRenderData& renderData = mRenderContext.getRenderData();
    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialUniform::Atlas:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D_ARRAY, renderData.atlas);
                break;
            case MaterialUniform::Fbo0:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getGeometryTexture());
                break;
            case MaterialUniform::FboLight:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getLightTexture());
                break;
            case MaterialUniform::FboDepth:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getDepthTexture());
                break;
            case MaterialUniform::FboNormals:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getNormalTexture());
                break;
            case MaterialUniform::FboRoughness:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getActiveGBuffer().getRoughnessTexture());
                break;
            case MaterialUniform::PrevFbo0:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getPrevFinalGBuffer().getGeometryTexture());
                break;
            case MaterialUniform::PrevFboDepth:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getPrevFinalGBuffer().getDepthTexture());
                break;
            case MaterialUniform::PixelDims: {
                const f32v2 pixelDims = 1.0f / mRenderContext.getCurrentFramebufferDims();
                glUniform2f(it.second, pixelDims.x, pixelDims.y);
                break;
            }
            case MaterialUniform::ZoomScale:
                glUniform1f(it.second, renderData.mainCamera->getScale());
                break;
            case MaterialUniform::FboZCutout:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getZCutoutGBuffer().getGeometryTexture());
                break;
            case MaterialUniform::CameraZAngle: {
                const f32 zAngle = atan2f(renderData.mainCamera->getFrontVector().y, renderData.mainCamera->getFrontVector().x) + M_PIF;
                glUniform1f(it.second, zAngle);
                break;
            }
            case MaterialUniform::SkyRotMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.skyRotMatrix[0][0]);
                break;
            case MaterialUniform::ScreenResolution: {
                const f32v2& pixelDims = mRenderContext.getCurrentFramebufferDims();
                glUniform2f(it.second, pixelDims.x, pixelDims.y);
                break;
            }
            case MaterialUniform::ShadowFrustumMatrices:
                glUniformMatrix4fv(it.second, renderData.shadowFrustumMatricesCount, false, &(*renderData.shadowFrustumMatrices)[0][0]);
                break;
            case MaterialUniform::ShadowCascadePlaneDistances:
                glUniform1fv(it.second, renderData.shadowFrustumMatricesCount, renderData.shadowCascadePlaneDistances);
                break;
            case MaterialUniform::ShadowMap:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D_ARRAY, renderData.shadowMap);
                break;
            case MaterialUniform::ShadowColor:
                glUniform3fv(it.second, 1, &sDebugOptions.mShadowColor[0]);
                break;
            case MaterialUniform::ShadowTexture:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getShadowTexture());
                break;
            case MaterialUniform::SSAOTexture:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, mRenderContext.getSSAOTexture());
                break;
            case MaterialUniform::SSAOColor:
                glUniform3fv(it.second, 1, &sDebugOptions.mAmbientOcclusionColor[0]);
                break;

        }
        static_assert((int)MaterialUniform::COUNT == 22, "Update for new uniform type");
    }
}
