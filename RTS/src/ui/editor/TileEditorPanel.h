#pragma once

#include <variant>
struct ModelDef;
struct MaterialGpuData;
struct MaterialHandle;
struct TileGrassData;
class MaterialShader;

DECL_VG(class GBuffer);

enum class TileEditorPanelResultCode {
    NONE,
    EDIT_MODEL,
    EDIT_MATERIAL,
    EDIT_FOLIAGE,
    COUNT
};

typedef std::variant<ModelDef*, std::unique_ptr<MaterialHandle>, TileGrassData*> TileEditorPanelResultVariant;

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
    VGTexture renderMaterialPreview(const MaterialShader* shader, int previewIndex, const MaterialGpuData& materialData);

    // Material preview
    VGVertexArray mPreviewVAO = 0;
    std::vector<std::unique_ptr<vg::GBuffer>> mMaterialPreviewGBuffers;
};

