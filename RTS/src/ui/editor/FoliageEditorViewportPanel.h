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
    void setGrassData(AssetID tileGrassId);

    const char* getViewportWindowName() const override { return "Foliage Editor"; }

private:
    void updateAndRenderInternal(f32 elapsedSec) override;

    void renderCenterPanel(i32AABB2* outImageRect) override;
    void renderGrassControls();

    std::vector<std::unique_ptr<GrassMesh>> mGrassMeshes;
    boost::container::flat_set<const GrassMesh*> mGrassMeshesSet;
    std::unique_ptr<GrassRenderer> mGrassRenderer;
    bool mDirtyFoliageMesh = true;
    i32v2 mDensityGradient = i32v2(255, 255);
};

