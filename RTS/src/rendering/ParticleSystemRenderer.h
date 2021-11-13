#pragma once

class ResourceManager;
class Camera3D;
class MaterialRenderer;
class ParticleSystem;
struct ParticleSystemData;

#include <Vorb/graphics/GBuffer.h>

class ParticleSystemRenderer
{
public:
    ParticleSystemRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims);
    ~ParticleSystemRenderer();

    void renderParticleSystems(const Camera3D& camera, vg::GBuffer* activeGbuffer, bool renderLitSystems);
private:
    void renderParticleSystem(const Camera3D& camera, const ParticleSystem& particleSystem);
    vg::GBuffer getOrCreateFramebufferForParticleSystem(const nString& name);
    void renderPostProcess(const ParticleSystemData& particleSystemData, vg::GBuffer& gBuffer);

    ResourceManager& mResourceManager;
    const MaterialRenderer& mMaterialRenderer;

    // For use in multipass
    std::map<nString, vg::GBuffer> mGBuffers;
    f32v2 mGbufferDims;
};

