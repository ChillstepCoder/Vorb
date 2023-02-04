#pragma once

#include <variant>
struct ModelDef;
struct MaterialGpuData;
struct MaterialHandle;
class MaterialShader;

DECL_VG(class GBuffer);

enum class TileEditorPanelResultCode {
    NONE,
    EDIT_MODEL,
    EDIT_MATERIAL,
    COUNT
};

typedef std::variant<ModelDef*, std::unique_ptr<MaterialHandle>> TileEditorPanelResultVariant;

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
    VGTexture renderMaterialPreview(const MaterialShader* shader, int previewIndex, const MaterialGpuData& materialData);

    // Material preview
    VGVertexArray mPreviewVAO = 0;
    std::vector<std::unique_ptr<vg::GBuffer>> mMaterialPreviewGBuffers;
};

