#pragma once

#include "rendering/material/MaterialData.h"

#include <Vorb/graphics/GLProgram.h>

DECL_VG(class Texture);

enum class MaterialShaderUniform {
    INVALID,
    Fbo0,
    FboDepth,
    FboNormals,
    FboRoughness,
    PrevFbo0,
    PrevFboDepth,
    PixelDims,
    SkyRotMatrix,
    ScreenResolution,
    ShadowColor,
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

extern MaterialShaderUniform lookupMaterialUniform(const nString& str);

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

struct MaterialShaderData {
    Array<MaterialTextureInputData> textures;
    nString vertexShaderName;
    nString fragmentShaderName;
    nString geometryShaderName;
    nString tessControlShaderName;
    nString tessEvalShaderName;
};
KEG_TYPE_DECL(MaterialShaderData);

struct MaterialTextureInput {
    VGTexture texture;
    VGUniform textureUniform;
};

class MaterialShader {
public:

    void use(OUT ui32& nextAvailableTextureIndex) const;
    // Doesn't dispose program
    void dispose();

    VGUniform getUniform(const char* name) const {
        return mProgram.getUniform(name);
    }
    const VGUniform* tryGetUniform(const char* name) const {
        return mProgram.tryGetUniform(name);
    }

    std::vector<std::pair<MaterialShaderUniform, VGUniform> > mUniforms;
    std::vector<MaterialTextureInput> mInputTextures;
    mutable vg::GLProgram mProgram; //  TODO: Handle
};
