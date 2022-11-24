#pragma once

struct ModelDef;
DECL_VG(class GBuffer);

class ModelEditorPanel
{
public:
    ModelEditorPanel();
    ~ModelEditorPanel();

    bool updateAndRender(const vg::GBuffer* activeGBuffer);

    void setModel(ModelDef& model) { mCurrentModel = &model; }

private:
    void updateCamera(f32v2 mouseDelta);
    void initGBuffer(f32v2 imageDims);
    void renderModelToTexture(f32 aspectRatio);

    f32v3 mCamPos = f32v3(4.0f, 0.0f, 4.0f);
    ModelDef* mCurrentModel = nullptr;
    std::unique_ptr<vg::GBuffer> mModelGBuffer = nullptr;
};

