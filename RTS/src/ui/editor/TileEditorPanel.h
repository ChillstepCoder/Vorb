#pragma once

#include <variant>
class ModelDef;
struct MaterialGpuData;
struct EditorMaterialHandle;
struct TileGrassData;
struct FishDef;
class MaterialShader;
class ParticleSystemDef;

DECL_VG(class GBuffer);

enum class TileEditorPanelResultCode {
    NONE,
    EDIT_MODEL,
    EDIT_MATERIAL,
    EDIT_FOLIAGE,
    EDIT_BIOME,
    EDIT_FISH,
    EDIT_PARTICLE,
    COUNT
};

typedef std::variant<ModelDef*, std::unique_ptr<EditorMaterialHandle>, TileGrassData*, FishDef*, AssetID> TileEditorPanelResultVariant;

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
    VGTexture renderMaterialPreview(const MaterialShader* shader, int previewIndex, const MaterialGpuData& materialData);

    // Material preview
    std::vector<std::unique_ptr<vg::GBuffer>> mMaterialPreviewGBuffers;
};

