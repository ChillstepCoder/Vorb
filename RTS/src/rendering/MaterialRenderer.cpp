#include "stdafx.h"
#include "MaterialRenderer.h"

#include "Material.h"
#include "rendering/QuadMesh.h"
#include "rendering/RenderContext.h"
#include "camera/ICamera.h"

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/FullQuadVBO.h>

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
    //  TODO: We are redundant with the texture uniform uploads. Can uniform buffer object save us here?
    // https://www.khronos.org/opengl/wiki/Uniform_Buffer_Object
    const GlobalRenderData& renderData = mRenderContext.getRenderData();
    // Bind uniforms
    for (auto&& it : material.mUniforms) {
        switch (it.first) {
            case MaterialUniform::Atlas:
                glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
                glUniform1i(it.second, nextAvailableTextureIndex++);
                glBindTexture(GL_TEXTURE_2D_ARRAY, renderData.atlas);
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
                glUniform3fv(it.second, 1, &renderData.sunPositionWorld[0]);
                break;
            case MaterialUniform::SunPositionCameraRelative:
                glUniform3fv(it.second, 1, &renderData.sunPositionCameraRelative[0]);
                break;
            case MaterialUniform::VMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.mainCamera->getViewMatrix()[0][0]);
                break;
            case MaterialUniform::InverseVMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.mainCamera->getInverseViewMatrix()[0][0]);
                break;
            case MaterialUniform::PMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.mainCamera->getProjectionMatrix()[0][0]);
                break;
            case MaterialUniform::InversePMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.mainCamera->getInverseProjectionMatrix()[0][0]);
                break;
            case MaterialUniform::VPMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.mainCamera->getVPMatrix()[0][0]);
                break;
            case MaterialUniform::InverseVPMatrix:
                glUniformMatrix4fv(it.second, 1, false, &renderData.mainCamera->getInverseVPMatrix()[0][0]);
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
            case MaterialUniform::PlayerPosWorld:
                glUniform3f(it.second, renderData.playerPos.x, renderData.playerPos.y, renderData.playerPos.z);
                break;
            case MaterialUniform::CameraRight:
                glUniform3fv(it.second, 1, &renderData.mainCamera->getRightVector()[0]);
                break;
            case MaterialUniform::CameraFront:
                glUniform3fv(it.second, 1, &renderData.mainCamera->getFrontVector()[0]);
                break;
            case MaterialUniform::CameraUp:
                glUniform3fv(it.second, 1, &renderData.mainCamera->getUpVector()[0]);
                break;
            case MaterialUniform::CameraPos:
                glUniform3fv(it.second, 1, &renderData.mainCamera->getPosition()[0]);
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
            case MaterialUniform::CameraZRange:
                glUniform2f(it.second, renderData.mainCamera->getZNear(), renderData.mainCamera->getZFar());
                break;
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
            case MaterialUniform::SunUp:
                glUniform3fv(it.second, 1, &renderData.sunUp[0]);
                break;
            case MaterialUniform::SunRight:
                glUniform3fv(it.second, 1, &renderData.sunRight[0]);
                break;
        }
        static_assert((int)MaterialUniform::COUNT == 37, "Update for new uniform type");
    }
}
