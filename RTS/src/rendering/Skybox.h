#pragma once

class Mesh;
class MaterialShader;
class CubemapDef;

class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    void init(const MaterialShader* material, const CubemapDef* skyTexture);
    void render(const f32m4& cameraMatrix);
    void renderPbr(const f32m4& cameraMatrix);
    void renderIrradianceDebug(const f32m4& cameraMatrix);
    void renderPrecomputedMapDebug(const f32m4& cameraMatrix, int baseLevel);

    void setCubemap(const CubemapDef* skyTexture);
    bool hasTexture() const { return mSkyTexture != nullptr; }
    const CubemapDef* getCubemap() const { return mSkyTexture; }

private:
    std::unique_ptr<Mesh> mSkyboxMesh;
    const MaterialShader* mMaterial = nullptr;
    const MaterialShader* mMaterialPbr = nullptr;
    const CubemapDef* mSkyTexture = nullptr;
};

