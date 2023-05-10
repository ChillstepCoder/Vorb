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
    void updateAndRenderControls(f32 ySize) override;

    void setGrassData(TileGrassData& grassData) { mGrassData = &grassData; }

private:
    void renderCenterPanel(i32AABB2* outImageRect) override;

    std::vector<std::unique_ptr<GrassMesh>> mGrassMeshes;
    boost::container::flat_set<const GrassMesh*> mGrassMeshesSet;
    std::unique_ptr<GrassRenderer> mGrassRenderer;
    TileGrassData* mGrassData = nullptr;
};

