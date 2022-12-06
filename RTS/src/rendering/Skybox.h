#pragma once

class QuadMesh;
class ICamera;
class MaterialRenderer;
class Mesh;
class MaterialShader;

class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    void init(const MaterialShader* material);
    void render();

private:
    std::unique_ptr<Mesh> mSkyboxMesh;
    const MaterialShader* mMaterial = nullptr;
};

