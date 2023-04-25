#pragma once

class MaterialShader;
class Mesh;

#include <Vorb/graphics/DepthState.h>

class MaterialRenderer {
public:
    MaterialRenderer() = delete;

    static void renderFullScreenQuad(const MaterialShader& material);
    static void renderMesh(const Mesh& mesh, const MaterialShader& material);
    static void renderMaterialToQuadWithTexture(const MaterialShader& material, VGTexture texture, const f32v4& worldSpaceRect);
    static void renderMaterialToQuadWithTextureBindless(const MaterialShader& material, VGTexture texture, ui32 textureIndex, const f32v4& worldSpaceRect);

    static void bindMaterialForRender(const MaterialShader& material, OUT ui32* nextAvailableTextureIndex = nullptr);

private:
    static void uploadUniforms(const MaterialShader& material, OUT ui32& nextAvailableTextureIndex);
};
