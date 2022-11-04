#pragma once

class QuadMesh;
class ICamera;
class MaterialRenderer;
class Mesh;
class Material;

class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    void init(const Material* material);
    void render();

private:
    std::unique_ptr<Mesh> mSkyboxMesh;
    const Material* mMaterial = nullptr;
};

