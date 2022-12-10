#pragma once

struct ModelDef;
DECL_VG(class GBuffer);

enum class ModelEditorPanelDrawMode {
    Default,
    Wireframe,
    Normals,
    COUNT
};

class ModelEditorPanel
{
public:
    ModelEditorPanel();
    ~ModelEditorPanel();

    bool updateAndRender(const vg::GBuffer* activeGBuffer);
    void updateAndRenderControls(f32 ySize);

    void setModel(ModelDef& model) { mCurrentModel = &model; }

private:
    void updateCamera(f32 aspectRatio);
    void initGBuffer(f32v2 imageDims);
    void renderModelToTexture();
    void renderGrid();

    VGVertexArray mGridVao = 0;
    ModelDef* mCurrentModel = nullptr;
    std::unique_ptr<vg::GBuffer> mModelGBuffer = nullptr;
    bool mDirtyModelData = false;
    int mLod = 0;

    ModelEditorPanelDrawMode mDrawMode = ModelEditorPanelDrawMode::Default;
};

