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
    FboZCutout,
    PlayerPosWorld,
    MousePosWorld,
    CameraRight,
    CameraFront,
    CameraPos,
    CameraZAngle,
    COUNT
};

extern MaterialUniform lookupMaterialUniform(const nString& str);

struct MaterialAtlasTextureInputData {
    nString textureName;
    nString uniformRectName;
    nString uniformPageName;
};
KEG_TYPE_DECL(MaterialAtlasTextureInputData);

struct MaterialTextureInputData {
    nString textureName;
    nString uniformName;
};
KEG_TYPE_DECL(MaterialTextureInputData);

struct MaterialData {
    Array<MaterialAtlasTextureInputData> atlasTextures;
    Array<MaterialTextureInputData> textures;
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
struct MaterialTextureInput {
    VGTexture texture;
    VGUniform textureUniform;
};

class Material {
public:

    void use(OUT ui32& nextAvailableTextureIndex) const;

    std::vector<std::pair<MaterialUniform, VGUniform> > mUniforms;
    std::vector<MaterialAtlasTextureInput> mInputAtlasTextures;
    std::vector<MaterialTextureInput> mInputTextures;
    mutable vg::GLProgram mProgram; //  TODO: Handle
};
