#pragma once

#include <variant>
class ModelDef;
struct MaterialGpuData;
struct TileGrassDef;
struct FishDef;
class MaterialShaderDef;
class ParticleSystemDef;

#include "resources/asset/AssetHandleBundle.h"

DECL_VG(class GBuffer);

enum class TileEditorPanelResultCode {
    NONE,
    EDIT_ASSET,
    EDIT_BIOME,
    COUNT
};

typedef std::variant<AssetDescriptor> TileEditorPanelResultVariant;

typedef std::pair<TileEditorPanelResultCode, TileEditorPanelResultVariant> TileEditorPanelResult;

class TileEditorPanel
{
public:
    TileEditorPanel();
    ~TileEditorPanel();

    TileEditorPanelResult updateAndRender(float ySize);

private:
    void updateAndRenderModelsTab(TileEditorPanelResult& result);
    void updateAndRenderMaterialsTab(TileEditorPanelResult& result);
    void updateAndRenderFoliageTab(TileEditorPanelResult& result);
    void updateAndRenderBiomeTab(TileEditorPanelResult& result);
    void updateAndRenderFishingTab(TileEditorPanelResult& result);
    void updateAndRenderParticlesTab(TileEditorPanelResult& result);
    VGTexture renderMaterialPreview(const MaterialShaderDef* shader, int previewIndex, const MaterialGpuData& materialData);

    // Material preview
    std::vector<std::unique_ptr<vg::GBuffer>> mMaterialPreviewGBuffers;
    AssetHandleBundle mForceLoadedAssets;
};

