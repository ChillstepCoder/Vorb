#pragma once

class Material;
class MeshBase;
class Mesh;

#include <Vorb/graphics/DepthState.h>

class MaterialRenderer {
public:
    MaterialRenderer() = delete;

    static void renderFullScreenQuad(const Material& material);
    static void renderMesh(const Mesh& mesh, const Material& material);
    static void renderMaterialToQuadWithTexture(const Material& material, VGTexture texture, const f32v4& worldSpaceRect);
    static void renderMaterialToQuadWithTextureBindless(const Material& material, VGTexture texture, ui32 textureIndex, const f32v4& worldSpaceRect);

    static void bindMaterialForRender(const Material& material, OUT ui32* nextAvailableTextureIndex = nullptr);

private:
    static void uploadUniforms(const Material& material, OUT ui32& nextAvailableTextureIndex);
};
