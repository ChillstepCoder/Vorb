#pragma once
\
#include <Vorb/graphics/GLProgram.h>

DECL_VG(class Texture);

enum class MaterialUniform {
    INVALID,
    Atlas,
    Time,
    TimeOfDay,
    SunColor,
    SunHeight,
    SunPosition,
    WMatrix,
    WVPMatrix,
    VPMatrix,
    Fbo0,
    FboLight,
    FboDepth,
    FboNormals,
    PrevFbo0,
    PrevFboDepth,
    PixelDims,
    ZoomScale,
    FboShadowHeight,
    FboZCutout,
    PlayerPosWorld,
    MousePosWorld,
    COUNT
};

extern MaterialUniform lookupMaterialUniform(const nString& str);

struct MaterialAtlasTextureInputData {
    nString textureName;
    nString uniformRectName;
    nString uniformPageName;
};
KEG_TYPE_DECL(MaterialAtlasTextureInputData);

struct MaterialData {
    Array<MaterialAtlasTextureInputData> atlasTextures;
    nString vertexShaderName;
    nString fragmentShaderName;
};
KEG_TYPE_DECL(MaterialData);

struct MaterialAtlasTextureInput {
    f32v4 uvRect;
    f32 page;
    VGUniform uvRectUniform;
    VGUniform pageUniform;
};

class Material {
public:

    void use() const;

    std::vector<std::pair<MaterialUniform, VGUniform> > mUniforms;
    std::vector<MaterialAtlasTextureInput> mInputAtlasTextures;
    mutable vg::GLProgram mProgram; //  TODO: Handle
};
