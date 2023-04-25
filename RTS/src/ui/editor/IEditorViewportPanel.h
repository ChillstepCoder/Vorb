#pragma once

DECL_VG(class GBuffer);

class SimpleCamera;
class CameraPositioner_FirstPerson;
class MaterialShader;
class Skybox;

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

    virtual bool updateAndRender() = 0;
    virtual void updateAndRenderControls(f32 ySize) = 0;

protected:
    void renderCenterPanel();

    // Virtual API
    virtual const MaterialShader* getShader() = 0;
    virtual void uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) = 0;
    virtual void renderMesh() = 0;

    VGTexture getFinalOutputTexture();
    void updateAndRenderSharedControls();
    void updateAndRenderTweakers();
    void updateCamera(f32 aspectRatio);
    void initGBuffers(ui32v2 imageDims);
    void renderGrid();
private:
    void uploadShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit);
    void renderPBRArray(const MaterialShader* shader);
protected:

    // Post processes
    void postProcessBlendTest();
    void postProcessEdgeTest();

    // Shared with all?
    std::unique_ptr<CameraPositioner_FirstPerson> positioner;
    std::unique_ptr<SimpleCamera> camera;

    VGVertexArray mGridVao = 0;
    std::unique_ptr<Skybox> mSkybox;
    EditorViewportDrawMode mDrawMode = EditorViewportDrawMode::PBRTest;
    // All editor viewport panels share the same GBuffers
    static std::unique_ptr<vg::GBuffer> sGBuffers[3];

    int mSelectedSkyboxIndex = 0;
    bool mShowSkyboxIrradiance = false;
    bool mShowSkyboxPrecomputedMap = false;
    int mPrecomputedLOD = 1;
    bool mRenderGrid = true;
    f32 mYaw = 0.0f;
    bool mRotate90 = true;
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
};

