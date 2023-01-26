#pragma once

class QuadMesh;
class ICamera;
class MaterialRenderer;
class Mesh;
class MaterialShader;
class Cubemap;

class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    void init(const MaterialShader* material, const Cubemap* skyTexture);
    void render(const f32m4& cameraMatrix);

    void setCubemap(const Cubemap* skyTexture);
    bool hasTexture() const { return mSkyTexture != nullptr; }

private:
    std::unique_ptr<Mesh> mSkyboxMesh;
    const MaterialShader* mMaterial = nullptr;
    const Cubemap* mSkyTexture = nullptr;
};

