#pragma once
#include "IEditorViewportPanel.h"

#include <boost/container/flat_set.hpp>

class GrassRenderer;
class GrassMesh;
struct TileGrassData;

class FoliageEditorViewportPanel : public IEditorViewportPanel {
public:
    FoliageEditorViewportPanel();
    ~FoliageEditorViewportPanel();

    bool updateAndRender() override;
    void updateAndRenderPrimaryControls(f32 ySize) override;

    void setGrassData(TileGrassData& grassData) { mGrassData = &grassData; mDirtyFoliageMesh = true; }

private:
    void renderCenterPanel(i32AABB2* outImageRect) override;
    void renderGrassControls();

    std::vector<std::unique_ptr<GrassMesh>> mGrassMeshes;
    boost::container::flat_set<const GrassMesh*> mGrassMeshesSet;
    std::unique_ptr<GrassRenderer> mGrassRenderer;
    TileGrassData* mGrassData = nullptr;
    bool mDirtyFoliageMesh = true;
    i32v2 mDensityGradient = i32v2(255, 255);
};

