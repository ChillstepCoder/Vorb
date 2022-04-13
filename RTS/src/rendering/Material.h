#pragma once

#include <Vorb/graphics/GLProgram.h>

DECL_VG(class Texture);

enum class MaterialUniform {
    INVALID,
    Atlas,
    Fbo0,
    FboDepth,
    FboNormals,
    FboRoughness,
    PrevFbo0,
    PrevFboDepth,
    PixelDims,
    ZoomScale,
    CameraZAngle,
    SkyRotMatrix,
    ScreenResolution,
    ShadowFrustumMatrices,
    ShadowMap,
    ShadowCascadePlaneDistances,
    ShadowColor,
    ShadowTexture,
    SSAOTexture,
    SSAOColor,
    DebugColor1,
    DebugColor2,
    DebugFloat1,
    DebugFloat2,
    DebugFloat3,
    DebugFloat4,
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
    nString geometryShaderName;
    nString tessControlShaderName;
    nString tessEvalShaderName;
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
    // Doesn't dispose program
    void dispose();

    const VGUniform& getUniform(const char* name) const {
        return mProgram.getUniform(name);
    }
    const VGUniform* tryGetUniform(const char* name) const {
        return mProgram.tryGetUniform(name);
    }

    std::vector<std::pair<MaterialUniform, VGUniform> > mUniforms;
    std::vector<MaterialAtlasTextureInput> mInputAtlasTextures;
    std::vector<MaterialTextureInput> mInputTextures;
    mutable vg::GLProgram mProgram; //  TODO: Handle
};
