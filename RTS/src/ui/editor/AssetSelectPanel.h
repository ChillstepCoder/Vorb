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

enum class AssetSelectPanelResultCode {
    NONE,
    EDIT_ASSET,
    COUNT
};

typedef std::pair<AssetSelectPanelResultCode, AssetDescriptor> AssetSelectPanelResult;

class AssetSelectPanel
{
public:
    AssetSelectPanel();
    ~AssetSelectPanel();

    AssetSelectPanelResult updateAndRender(float ySize);

private:
    void updateAndRenderModelsTab(AssetSelectPanelResult& result);
    void updateAndRenderMaterialsTab(AssetSelectPanelResult& result);
    void updateAndRenderFoliageTab(AssetSelectPanelResult& result);
    void updateAndRenderBiomeTab(AssetSelectPanelResult& result);
    void updateAndRenderFishingTab(AssetSelectPanelResult& result);
    void updateAndRenderParticlesTab(AssetSelectPanelResult& result);
    VGTexture renderMaterialPreview(const MaterialShaderDef* shader, int previewIndex, const MaterialGpuData& materialData);

    // Material preview
    std::vector<std::unique_ptr<vg::GBuffer>> mMaterialPreviewGBuffers;
    AssetHandleBundle mForceLoadedAssets;
};

