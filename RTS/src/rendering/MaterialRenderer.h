#pragma once

class RenderContext;
class Material;
class QuadMesh;

#include <Vorb/graphics/FullQuadVBO.h>

class MaterialRenderer {
public:

    MaterialRenderer(const RenderContext& renderContext);
    ~MaterialRenderer();

    void renderMaterialToScreen(const Material& material);
    void renderQuadMesh(const QuadMesh& quadMesh, const Material& material);

private:
    void uploadUniforms(const Material& material);
    // TODO: Subsections, like a UI render
    vg::FullQuadVBO mScreenVBO;
    const RenderContext& mRenderContext;
};
