#pragma once

class ResourceManager;
class Camera2D;
class MaterialRenderer;
class ParticleSystem;

class ParticleSystemRenderer
{
public:
    ParticleSystemRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
    ~ParticleSystemRenderer();

    void renderParticleSystems(const Camera2D& camera);
private:
    void renderParticleSystem(const Camera2D& camera, const ParticleSystem& particleSystem);

    ResourceManager& mResourceManager;
    const MaterialRenderer& mMaterialRenderer;
};

