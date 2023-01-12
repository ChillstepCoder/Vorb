#include "stdafx.h"
#include "MaterialRenderer.h"

#include "MaterialShader.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/Mesh.h"
#include "camera/ICamera.h"

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/FullQuadVBO.h>
#include <Vorb/graphics/GBuffer.h>

#include "options/DebugOptions.h"

void MaterialRenderer::renderFullScreenQuad(const MaterialShader& material) {

    bindMaterialForRender(material, nullptr);

    sGlobalFullQuadVBO.draw();
}

void MaterialRenderer::renderMesh(const Mesh& mesh, const MaterialShader& material) {
    bindMaterialForRender(material, nullptr);

    mesh.draw();
}

void MaterialRenderer::renderMaterialToQuadWithTexture(const MaterialShader& material, VGTexture texture, const f32v4& worldSpaceRect) {
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

void MaterialRenderer::renderMaterialToQuadWithTextureBindless(const MaterialShader& material, VGTexture texture, ui32 textureIndex, const f32v4& worldSpaceRect) {
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

void MaterialRenderer::bindMaterialForRender(const MaterialShader& material, OUT ui32* nextAvailableTextureIndex /* =nullptr */) {
    ui32 availableTextureIndex = 0;
    material.use(availableTextureIndex);
    uploadUniforms(material, availableTextureIndex);
    if (nextAvailableTextureIndex) {
        *nextAvailableTextureIndex = availableTextureIndex;
    }
}

// TODO: Batch upload uniforms so we dont do it multiple times redundantly
void MaterialRenderer::uploadUniforms(const MaterialShader& material, OUT ui32& nextAvailableTextureIndex) {
    RenderContext& renderContext = RenderContext::getInstance();
    const GlobalRenderData& renderData = renderContext.getRenderData();
    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialShaderUniform::Fbo0:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getActiveGBuffer().getAlbedoTexture());
                break;
            case MaterialShaderUniform::FboDepth:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getActiveGBuffer().getDepthTexture());
                break;
            case MaterialShaderUniform::FboNormals:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getActiveGBuffer().getNormalTexture());
                break;
            case MaterialShaderUniform::FboRoughness:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getActiveGBuffer().getTertiaryTexture());
                break;
            case MaterialShaderUniform::PrevFbo0:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getPrevFinalGBuffer().getAlbedoTexture());
                break;
            case MaterialShaderUniform::PrevFboDepth:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getPrevFinalGBuffer().getDepthTexture());
                break;
            case MaterialShaderUniform::PixelDims: {
                const f32v2 pixelDims = 1.0f / f32v2(renderContext.getCurrentFramebufferDims());
                glUniform2f(it.second, pixelDims.x, pixelDims.y);
                break;
            }
            case MaterialShaderUniform::ZoomScale:
                // TODO: remove this
                glUniform1f(it.second, 1.0f);
                break;
            case MaterialShaderUniform::CameraZAngle: {
                const f32 zAngle = atan2f(renderData.mainCamera->getFrontVector().y, renderData.mainCamera->getFrontVector().x) + M_PIF;
                glUniform1f(it.second, zAngle);
                break;
            }
            case MaterialShaderUniform::SkyRotMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.skyRotMatrix[0][0]);
                break;
            case MaterialShaderUniform::ScreenResolution: {
                const f32v2& pixelDims = renderContext.getCurrentFramebufferDims();
                glUniform2f(it.second, pixelDims.x, pixelDims.y);
                break;
            }
            case MaterialShaderUniform::ShadowFrustumMatrices:
                glUniformMatrix4fv(it.second, renderData.shadowFrustumMatricesCount, false, &(*renderData.shadowFrustumMatrices)[0][0]);
                break;
            case MaterialShaderUniform::ShadowCascadePlaneDistances:
                glUniform1fv(it.second, renderData.shadowFrustumMatricesCount, renderData.shadowCascadePlaneDistances);
                break;
            case MaterialShaderUniform::ShadowMap:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D_ARRAY, renderData.shadowMap);
                break;
            case MaterialShaderUniform::ShadowColor:
                glUniform3fv(it.second, 1, &sDebugOptions.mShadowColor[0]);
                break;
            case MaterialShaderUniform::ShadowTexture:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getShadowTexture());
                break;
            case MaterialShaderUniform::SSAOTexture:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D, renderContext.getSSAOTexture());
                break;
            case MaterialShaderUniform::SSAOColor:
                glUniform3fv(it.second, 1, &sDebugOptions.mSSAOColor[0]);
                break;
            case MaterialShaderUniform::DebugColor1:
                glUniform3fv(it.second, 1, &sDebugOptions.mDebugColor01[0]);
                break;
            case MaterialShaderUniform::DebugColor2:
                glUniform3fv(it.second, 1, &sDebugOptions.mDebugColor02[0]);
                break;
            case MaterialShaderUniform::DebugFloat1:
                glUniform1f(it.second, sDebugOptions.mDebugFloat01);
                break;
            case MaterialShaderUniform::DebugFloat2:
                glUniform1f(it.second, sDebugOptions.mDebugFloat02);
                break;
            case MaterialShaderUniform::DebugFloat3:
                glUniform1f(it.second, sDebugOptions.mDebugFloat03);
                break;
            case MaterialShaderUniform::DebugFloat4:
                glUniform1f(it.second, sDebugOptions.mDebugFloat04);
                break;
        }
        static_assert((int)MaterialShaderUniform::COUNT == 25, "Update for new uniform type");
    }
}
