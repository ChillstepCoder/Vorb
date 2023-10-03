#pragma once

class Mesh;
class MaterialShaderDef;

#include "definitions/rendering/CubemapDef.h"

class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    void init(AssetHandlePtr<CubemapDef>&& skyCubemap);
    void render(const f32m4& cameraMatrix);
    void renderPbr(const f32m4& cameraMatrix);
    void renderIrradianceDebug(const f32m4& cameraMatrix);
    void renderPrecomputedMapDebug(const f32m4& cameraMatrix, int baseLevel);

    void setCubemap(AssetHandlePtr<CubemapDef>&& skyCubemap);
    bool hasTexture() const { return mSkyCubemap != nullptr; }
    const CubemapDef* tryGetCubemap() const { return mSkyCubemap ? mSkyCubemap->tryGetAsset() : nullptr; }

private:
    std::unique_ptr<Mesh> mSkyboxMesh;
    AssetHandlePtr<MaterialShaderDef> mMaterialShader;
    AssetHandlePtr<MaterialShaderDef> mMaterialShaderPbr;
    AssetHandlePtr<CubemapDef> mSkyCubemap;
};

