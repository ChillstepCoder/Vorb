#pragma once

DECL_VG(class GBuffer);

class SimpleCamera;
class CameraPositioner_FirstPerson;

enum class EditorViewportDrawMode {
    Lit = 0,
    Unlit = 1,
    Normals = 2,
    UVs = 3,
    BlendTest = 4,
    EdgeTest = 5,
    Wireframe = 6, // Always last
    COUNT
};
static_assert(e_cast(EditorViewportDrawMode::COUNT) == 7, "Copy to data/shaders/editor/editor_util.glsl");

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
    void initGBuffers(ui32v2 imageDims);
    void renderGrid();

    // Shared with all?
    std::unique_ptr<CameraPositioner_FirstPerson> positioner;
    std::unique_ptr<SimpleCamera> camera;

    VGVertexArray mGridVao = 0;
    std::unique_ptr<vg::GBuffer> mGBuffers[3];
    EditorViewportDrawMode mDrawMode = EditorViewportDrawMode::Lit;
};

