#pragma once

#include <Vorb/io/Keg.h>
#include <Vorb/graphics/GLProgram.h>

DECL_VG(class Texture);

enum class MaterialUniform {
    INVALID,
    Atlas,
    Time,
    WMatrix,
    WVPMatrix,
    VPMatrix,
    COUNT
};

extern MaterialUniform lookupMaterialUniform(const nString& str);

struct MaterialData {
    Array<nString> inputTextureNames;
    nString        shaderName;
};
KEG_TYPE_DECL(MaterialData);

class Material {
public:

    void use() const;

    std::vector<std::pair<MaterialUniform, VGUniform> > mUniforms;
    std::vector<vg::Texture> mInputTextures;
    mutable vg::GLProgram mProgram; //  TODO: Handle
};
