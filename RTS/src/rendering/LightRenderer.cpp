#include "stdafx.h"
#include "LightRenderer.h"

#include <glm/mat3x3.hpp>

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "ResourceManager.h"
#include "Camera2D.h"

static_assert((int)LightShape::Count == 1, "Update this file to handle new light shape");
static_assert((int)LightAttenuationType::Count == 1, "Update this file to handle new attenuation type");

LightRenderer::LightRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer)
{
}

LightRenderer::~LightRenderer() {
    if (mSharedPointLightMesh.mVao) {
        glDeleteBuffers(2, mSharedPointLightMesh.mBuffers);
        glDeleteVertexArrays(1, &mSharedPointLightMesh.mVao);
    }
}

void LightRenderer::RenderLight(const f32v2& position, const LightData& lightData, const Camera2D& camera) const {

    mMaterialRenderer.bindMaterialForRender(*mPointLightMaterial);

    // Upload light uniforms
    glUniform1f(mPointLightMaterial->mProgram.getUniform("InnerRadius"), lightData.mInnerRadiusCoef);
    glUniform3f(mPointLightMaterial->mProgram.getUniform("Color"), lightData.mColor.r / 255.0f, lightData.mColor.g / 255.0f, lightData.mColor.b / 255.0f);
    glUniform1f(mPointLightMaterial->mProgram.getUniform("Intensity"), lightData.mIntensity);
    glUniform2f(mPointLightMaterial->mProgram.getUniform("Position"), position.x, position.y);
    glUniform1f(mPointLightMaterial->mProgram.getUniform("Scale"), lightData.mOuterRadius);

    // Bind mesh
    glBindVertexArray(mSharedPointLightMesh.mVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mSharedPointLightMesh.mIb);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void LightRenderer::InitPostLoad()
{
    mPointLightMaterial = mResourceManager.getMaterialManager().getMaterial("point_light");
    assert(mPointLightMaterial);

    InitSharedMesh();
}

void LightRenderer::InitSharedMesh()
{
    glGenBuffers(2, mSharedPointLightMesh.mBuffers);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mSharedPointLightMesh.mIb);
    const ui32 inds[6] = { 0, 1, 3, 0, 3, 2 };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, mSharedPointLightMesh.mVb);
    const f32 points[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

    glGenVertexArrays(1, &mSharedPointLightMesh.mVao);
    glBindVertexArray(mSharedPointLightMesh.mVao);
    glEnableVertexAttribArray(0);
    // Vertex shader input, simple position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, mSharedPointLightMesh.mVb);
    glBindVertexArray(0);
}
