#pragma once

class RenderContext;
class Material;
class QuadMesh;

#include <Vorb/graphics/FullQuadVBO.h>
#include <Vorb/graphics/DepthState.h>

class MaterialRenderer {
public:

    MaterialRenderer(const RenderContext& renderContext);
    ~MaterialRenderer();

    void renderMaterialToScreen(const Material& material) const;
    void renderQuadMesh(const QuadMesh& quadMesh, const Material& material, const vg::DepthState& depthState = vg::DepthState::FULL) const;
    void bindMaterialForRender(const Material& material) const;

private:
    void uploadUniforms(const Material& material) const;
    // TODO: Subsections, like a UI render
    vg::FullQuadVBO mScreenVBO;
    const RenderContext& mRenderContext;
};
