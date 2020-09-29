#pragma once

class RenderContext;
class Material;

#include <Vorb/graphics/FullQuadVBO.h>

class MaterialRenderer {
public:

    MaterialRenderer();
    ~MaterialRenderer();

    void renderMaterialToScreen(const Material& material, const RenderContext& renderContext);

    // TODO: Subsections, like a UI render
    vg::FullQuadVBO mScreenVBO;
};

