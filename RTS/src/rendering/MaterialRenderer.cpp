#include "stdafx.h"
#include "MaterialRenderer.h"

#include "MaterialShaderDef.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/MeshDrawer.h"
#include "camera/ICamera.h"

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/GBuffer.h>

#include "options/DebugOptions.h"

void MaterialRenderer::renderFullScreenQuad(const MaterialShaderDef& material) {

    bindMaterialShaderForRender(material, nullptr);

    sGlobalFullTriangleVAO.draw();
}

void MaterialRenderer::renderMesh(const Mesh& mesh, const MaterialShaderDef& material) {
    bindMaterialShaderForRender(material, nullptr);

    MeshDrawer::draw(mesh.mGpuData);
}

void MaterialRenderer::renderMaterialToQuadWithTexture(const MaterialShaderDef& material, VGTexture texture, const f32v4& worldSpaceRect) {
    assert(texture);
    ui32 textureIndex;
    bindMaterialShaderForRender(material, &textureIndex);

    // TODO: Uniform buffer object for static uniforms
    VGUniform textureUniform = material.mProgram.getUniform("Texture");
    glActiveTexture(GL_TEXTURE0 + textureIndex);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(textureUniform, textureIndex);

    // TODO: Cache so we dont need a string lookup? (I think it just has to be cached on outer loop)
    VGUniform rectUniform = material.mProgram.getUniform("Rect");
    glUniform4fv(rectUniform, 1, &(worldSpaceRect.x));

    sGlobalFullTriangleVAO.draw();
}

void MaterialRenderer::renderMaterialToQuadWithTextureBindless(const MaterialShaderDef& material, VGTexture texture, ui32 textureIndex, const f32v4& worldSpaceRect) {
    assert(texture);
    // TODO: Uniform buffer object for static uniforms
    VGUniform textureUniform = material.mProgram.getUniform("Texture");
    glActiveTexture(GL_TEXTURE0 + textureIndex);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(textureUniform, textureIndex);

    // TODO: Cache so we dont need a string lookup? (I think it just has to be cached on outer loop)
    VGUniform rectUniform = material.mProgram.getUniform("Rect");
    glUniform4fv(rectUniform, 1, &(worldSpaceRect.x));

    sGlobalFullTriangleVAO.draw();
}

void MaterialRenderer::bindMaterialShaderForRender(const MaterialShaderDef& material, OUT ui32* nextAvailableTextureIndex /* =nullptr */) {
    ui32 availableTextureIndex = 0;
    material.use(availableTextureIndex);
    uploadUniforms(material, availableTextureIndex);
    if (nextAvailableTextureIndex) {
        *nextAvailableTextureIndex = availableTextureIndex;
    }
}

// TODO: Batch upload uniforms so we dont do it multiple times redundantly
void MaterialRenderer::uploadUniforms(const MaterialShaderDef& material, OUT ui32& nextAvailableTextureIndex) {
    RenderContext& renderContext = RenderContext::getInstance();
    const GlobalRenderData& renderData = renderContext.getRenderData();
    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialShaderUniform::Fbo0:
                glBindTextureUnit(nextAvailableTextureIndex, renderContext.getActiveGBuffer().getAlbedoTexture());
                glUniform1i(it.second, nextAvailableTextureIndex++);
                break;
            case MaterialShaderUniform::FboDepth:
                glBindTextureUnit(nextAvailableTextureIndex, renderContext.getActiveGBuffer().getDepthTexture());
                glUniform1i(it.second, nextAvailableTextureIndex++);
                break;
            case MaterialShaderUniform::FboNormals:
                glBindTextureUnit(nextAvailableTextureIndex, renderContext.getActiveGBuffer().getNormalTexture());
                glUniform1i(it.second, nextAvailableTextureIndex++);
                break;
            case MaterialShaderUniform::FboRoughness:
                glBindTextureUnit(nextAvailableTextureIndex, renderContext.getActiveGBuffer().getTertiaryTexture());
                glUniform1i(it.second, nextAvailableTextureIndex++);
                break;
            case MaterialShaderUniform::PrevFbo0:
                glBindTextureUnit(nextAvailableTextureIndex, renderContext.getPrevFinalGBuffer().getAlbedoTexture());
                glUniform1i(it.second, nextAvailableTextureIndex++);
                break;
            case MaterialShaderUniform::PrevFboDepth:
                glBindTextureUnit(nextAvailableTextureIndex, renderContext.getPrevFinalGBuffer().getDepthTexture());
                glUniform1i(it.second, nextAvailableTextureIndex++);
                break;
            case MaterialShaderUniform::PixelDims: {
                const f32v2 pixelDims = 1.0f / f32v2(renderContext.getCurrentFramebufferDims());
                glUniform2f(it.second, pixelDims.x, pixelDims.y);
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
            case MaterialShaderUniform::ShadowColor:
                glUniform3fv(it.second, 1, &sDebugOptions.mShadowColor[0]);
                break;
            case MaterialShaderUniform::SSAOTexture:
                assert(false);
                //glBindTextureUnit(nextAvailableTextureIndex, renderContext.getSSAOTexture());
                //glUniform1i(it.second, nextAvailableTextureIndex++);
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
        static_assert((int)MaterialShaderUniform::COUNT == 19, "Update for new uniform type");
    }
}
