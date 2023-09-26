#pragma once

class MaterialShaderDef;
class Mesh;

#include <Vorb/graphics/DepthState.h>

class MaterialRenderer {
public:
    MaterialRenderer() = delete;

    static void renderFullScreenQuad(const MaterialShaderDef& material);
    static void renderMesh(const Mesh& mesh, const MaterialShaderDef& material);
    static void renderMaterialToQuadWithTexture(const MaterialShaderDef& material, VGTexture texture, const f32v4& worldSpaceRect);
    static void renderMaterialToQuadWithTextureBindless(const MaterialShaderDef& material, VGTexture texture, ui32 textureIndex, const f32v4& worldSpaceRect);

    static void bindMaterialShaderForRender(const MaterialShaderDef& material, OUT ui32* nextAvailableTextureIndex = nullptr);

private:
    static void uploadUniforms(const MaterialShaderDef& material, OUT ui32& nextAvailableTextureIndex);
};
