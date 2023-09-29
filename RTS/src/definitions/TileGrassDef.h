#pragma once

#include "tile/TileGrass.h"

#include "rendering/mesh/TileGrassMeshType.h"

#include "generation/NoiseFunction.hpp"

class TileGrassDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(TileGrassDef);

    StrToken mAlphaMaskTextureName;
    StrToken mTextureName;
    f32v2 mSizeMults = f32v2(1.0f);
    f32v2 mHeightVariance = f32v2(0.0f, 0.0f);
    f32v2 mZOffsetVariance = f32v2(0.0f, 0.0f);
    f32 mLeanVariance = 0.0f;
    TileGrassMeshType mMeshType;
    ui8 mDensity = 8; //  MAX_GRASS_DETAIL
    ui8 mNumTextures = 1;
    MaterialID mMaterialID = INVALID_MATERIAL_ID;
    bool mUseGradientColor = false;
    NoiseFunction mNoiseFunction = NoiseFunction(CStrToken("Grass"), NoiseFunctionType::Standard, 6, 0.7, 0.05, { 1200.0, -1200.0 }, 1.0, 0.0);
};
SERIALIZABLE_SIMPLE(TileGrassDef, 
    make_field(o.mAlphaMaskTextureName, "alpha_masks"sv),
    make_field(o.mTextureName, "textures"sv),
    make_field(o.mSizeMults, "size"sv),
    make_field(o.mHeightVariance, "height_variance"sv),
    make_field(o.mZOffsetVariance, "z_offset_variance"sv),
    make_field(o.mLeanVariance, "lean_variance"sv),
    make_field(o.mMeshType, "mesh_type"sv),
    make_field(o.mDensity, "density"sv),
    make_field(o.mNumTextures, "num_textures"sv),
    make_field(o.mUseGradientColor, "use_gradient_color"sv),
    make_field(o.mNoiseFunction, "noise_function"sv)
);
