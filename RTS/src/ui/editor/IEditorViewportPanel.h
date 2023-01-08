#pragma once

DECL_VG(class GBuffer);

class SimpleCamera;
class CameraPositioner_FirstPerson;


enum class EditorViewportDrawMode {
    Default,
    Wireframe,
    Normals,
    COUNT
};

class IEditorViewportPanel
{
public:
    IEditorViewportPanel();
    virtual ~IEditorViewportPanel();

    virtual bool updateAndRender() = 0;
    virtual void updateAndRenderControls(f32 ySize) = 0;

protected:
    void updateAndRenderDrawModeControl();
    void updateCamera(f32 aspectRatio);
    void initGBuffer(f32v2 imageDims);
    void renderGrid();

    // Shared with all?
    std::unique_ptr<CameraPositioner_FirstPerson> positioner;
    std::unique_ptr<SimpleCamera> camera;

    VGVertexArray mGridVao = 0;
    std::unique_ptr<vg::GBuffer> mGBuffer = nullptr;
    EditorViewportDrawMode mDrawMode = EditorViewportDrawMode::Default;
};

