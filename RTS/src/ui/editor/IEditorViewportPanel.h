#pragma once

DECL_VG(class GBuffer);

class SimpleCamera;
class CameraPositioner_FirstPerson;
class MaterialShaderDef;
class Skybox;

// Helper forward declare
namespace ImguiUtil {
    class RenameAssetPopup;
    class ConfirmDeletePopup;
    class CustomSelectorPopup;
    class AssetSelectorPopup;
}

enum class EditorViewportDrawMode {
    Lit = 0,
    Unlit = 1,
    Normals = 2,
    Tangents = 3,
    AO = 4,
    Metallic = 5,
    Roughness = 6,
    UVs = 7, // This and back uses lighting
    BlendTest = 8,
    EdgeTest = 9,
    PBRTest = 10,
    Wireframe = 11, // Always last
    COUNT
};
static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12, "Copy to data/shaders/editor/editor_util.glsl");

class IEditorViewportPanel
{
public:
    IEditorViewportPanel();
    virtual ~IEditorViewportPanel();

    virtual bool updateAndRender(f32 elapsedSec) = 0;
    virtual void updateAndRenderPrimaryControls(f32 ySize) = 0;
    // Optional
    virtual bool updateAndRenderSecondaryControls(f32 ySize) { return false; }
    // Optional
    virtual bool updateAndRenderTertiaryControls(f32 ySize) { return false; }
    // Optional
    virtual bool hasBottomControls() const { return false; }
    virtual void updateAndRenderBottomControls() {  }

    virtual void onEnter() { mDidJustEnter = true; onEnterInternal(); }
    virtual void onExit() { mDidJustEnter = false; }

    f32v3 getCameraPosition() const;
    f32v3 getCameraDirection() const;
    f32v3 getCameraRight() const;
    f32v3 getCameraUp() const;

protected:
    virtual void renderCenterPanel(i32AABB2* outImageRect);
    virtual void clearFramebuffers();
    virtual void renderSkybox();
    virtual void renderCenterPanelImage(i32AABB2* outImageRect, VGTexture displayTexture);
    virtual void onEnterInternal() {}
    void updateFramebufferAndLazyInit(const i32v2& framebufferDims);

    // Virtual API
    virtual const MaterialShaderDef* getShader() { return nullptr; };
    virtual void uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) {};
    virtual void renderMesh() {};

    virtual VGTexture getFinalOutputTexture();
    void updateAndRenderSharedControls();
    void updateAndRenderTweakers();
    void updateCamera(f32 aspectRatio);
    void initGBuffers(ui32v2 imageDims);
    void renderGrid(const f32m4& VP);
private:
    void uploadShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit);
    void renderPBRArray(const MaterialShaderDef* shader);
protected:

    // Post processes
    void postProcessBlendTest();
    void postProcessEdgeTest();

    // Shared with all?
    std::unique_ptr<CameraPositioner_FirstPerson> mCameraPositioner;
    std::unique_ptr<SimpleCamera> mCamera;

    AssetHandlePtr<MaterialShaderDef> mGridMaterial;
    VGVertexArray mGridVao = 0;
    std::unique_ptr<Skybox> mSkybox;
    EditorViewportDrawMode mDrawMode = EditorViewportDrawMode::PBRTest;
    // All editor viewport panels share the same GBuffers
    static std::unique_ptr<vg::GBuffer> sGBuffers[3];
    i32v2 mCurrentGbufferDims = i32v2(0);

    int mSelectedSkyboxIndex = 0;
    bool mShowSkyboxIrradiance = false;
    bool mShowSkyboxPrecomputedMap = false;
    int mPrecomputedLOD = 1;
    bool mRenderGrid = true;
    f32 mYaw = 0.0f;
    bool mRotate90 = false;
    bool mDisableBackfaceCulling = true;

    // Blend test
    int mBlendTestPasses = 1;
    f32 mBlendTestRadius = 7.0f;
    f32 mBlendTestNormThreshold = 0.016f;
    f32 mBlendTestDepthThreshold = 0.104f;
    int mBlendTestDisplayMode = 0;
    bool mBlendTestShowVariance = 0;
    bool mBlendTestShowEdges = 0;
    bool mBlendTestDisable = 0;

    // Edge test //TODO: She likes this blurrier
    float mEdgeTestThreshold = 0.08f;
    f32 mEdgeTestDepthThreshold = 0.05f;
    int mEdgeTestDisplayMode = 0;
    int mEdgeSize = 10;
    int mEdgeBlendPasses = 3;
    f32 mEdgeBlendRadius = 1.315f;
    bool mEdgeTestDisable = 0;
    bool mEdgeTestShowEdges = 0;

    // PBR test
    bool mOverrideMetallicRoughness = false;
    float mMetallic = 0.5f;
    float mRoughness = 0.5f;
    float mHeightScale = 0.18f;
    float mExposure = 1.0f;
    float mAmbient = 1.0f;
    float mSunIntensity = 3.0f;
    f32v2 mLightDir = f32v2(1.0f, 1.0f);
    f32v3 mLightColor = f32v3(1.0f, 0.8f, 0.8f);
    bool mRenderArray = false;
    bool mFollowAxis = false;
    bool mDidJustEnter = false;

    // Config
    bool mShowDrawModeDropdown = true;
    f32v4 mClearColor = f32v4(0.3f, 0.3f, 0.3f, 1.0f);
};

