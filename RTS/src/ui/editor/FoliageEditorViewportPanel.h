#pragma once
#include "AssetEditorViewportPanel.h"

#include <boost/container/flat_set.hpp>

#include "definitions/TileGrassDef.h"

class GrassRenderer;
class GrassMesh;

class FoliageEditorViewportPanel : public AssetEditorViewportPanel<TileGrassDef> {
public:
    FoliageEditorViewportPanel();
    ~FoliageEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Foliage Editor"; }

private:

    void renderCenterPanel(i32AABB2* outImageRect) override;
    void renderGrassControls();

    std::vector<std::unique_ptr<GrassMesh>> mGrassMeshes;
    boost::container::flat_set<const GrassMesh*> mGrassMeshesSet;
    std::unique_ptr<GrassRenderer> mGrassRenderer;
    i32v2 mDensityGradient = i32v2(255, 255);
};

