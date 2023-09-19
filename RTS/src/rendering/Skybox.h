#pragma once

class Mesh;
class MaterialShader;

#include "definitions/rendering/CubemapDef.h"

class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    void init(const MaterialShader* material, AssetHandlePtr<CubemapDef>&& skyCubemap);
    void render(const f32m4& cameraMatrix);
    void renderPbr(const f32m4& cameraMatrix);
    void renderIrradianceDebug(const f32m4& cameraMatrix);
    void renderPrecomputedMapDebug(const f32m4& cameraMatrix, int baseLevel);

    void setCubemap(AssetHandlePtr<CubemapDef>&& skyCubemap);
    bool hasTexture() const { return mSkyCubemap != nullptr; }
    const CubemapDef* getCubemap() const { return mSkyCubemap ? mSkyCubemap->tryGetAsset() : nullptr; }

private:
    std::unique_ptr<Mesh> mSkyboxMesh;
    const MaterialShader* mMaterial = nullptr;
    const MaterialShader* mMaterialPbr = nullptr;
    AssetHandlePtr<CubemapDef> mSkyCubemap;
};

