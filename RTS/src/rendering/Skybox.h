#pragma once

class QuadMesh;
class ICamera;
class MaterialRenderer;
class Material;

class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    void init(const Material* material);
    void render(const MaterialRenderer& materialRenderer);

private:
    std::unique_ptr<QuadMesh> mSkyboxMesh;
    const Material* mMaterial = nullptr;
};

