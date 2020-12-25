#pragma once

class RenderContext;
class Material;
class QuadMesh;

#include <Vorb/graphics/FullQuadVBO.h>
#include <Vorb/graphics/DepthState.h>

enum FboGeometryLayers {
    FBO_GEOMETRY_COLOR  = 0,
    FBO_GEOMETRY_NORMAL = 1
};

class MaterialRenderer {
public:

    MaterialRenderer(const RenderContext& renderContext);
    ~MaterialRenderer();

    void renderFullScreenQuad(const Material& material) const;
    void renderQuadMesh(const QuadMesh& quadMesh, const Material& material, const vg::DepthState& depthState = vg::DepthState::FULL) const;
    void renderMaterialToQuadWithTexture(const Material& material, VGTexture texture, const f32v4& worldSpaceRect);
    void renderMaterialToQuadWithTextureBindless(const Material& material, VGTexture texture, ui32 textureIndex, const f32v4& worldSpaceRect);

    void bindMaterialForRender(const Material& material, OUT ui32* nextAvailableTextureIndex = nullptr) const;

private:
    void uploadUniforms(const Material& material, OUT ui32* nextAvailableTextureIndex) const;
    // TODO: Subsections, like a UI render
    vg::FullQuadVBO mQuadVBO;
    const RenderContext& mRenderContext;
};
