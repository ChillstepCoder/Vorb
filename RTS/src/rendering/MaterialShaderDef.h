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

struct MaterialTextureInputData {
    StrToken textureName;
    nString uniformName;
};
SERIALIZABLE_SIMPLE(MaterialTextureInputData,
    make_field(o.textureName, "name"sv),
    make_field(o.uniformName, "uniform"sv)
);

struct MaterialTextureInput {
    VGTexture texture;
    VGUniform textureUniform;
};

class MaterialShaderDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(MaterialShaderDef, AssetType::MaterialShader);

    void use(OUT ui32& nextAvailableTextureIndex) const;
    void useCompute() const;
    static void unuse();
    // Doesn't dispose program
    //void dispose();

    VGUniform getUniform(const char* name) const {
        return mProgram.getUniform(name);
    }
    const VGUniform* tryGetUniform(const char* name) const {
        return mProgram.tryGetUniform(name);
    }

    std::vector<std::pair<MaterialShaderUniform, VGUniform> > mUniforms;
    std::vector<MaterialTextureInput> mInputTextures;
    mutable vg::GLProgram mProgram; //  TODO: Handle
    bool mIsCompute = false;
};
