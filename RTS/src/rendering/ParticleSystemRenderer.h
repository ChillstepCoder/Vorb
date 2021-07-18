#pragma once

class ResourceManager;
class Camera2D;
class MaterialRenderer;
class ParticleSystem;
struct ParticleSystemData;

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/FullQuadVBO.h>

class ParticleSystemRenderer
{
public:
    ParticleSystemRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims);
    ~ParticleSystemRenderer();

    void renderParticleSystems(const Camera2D& camera, vg::GBuffer* activeGbuffer, bool renderLitSystems);
private:
    void renderParticleSystem(const Camera2D& camera, const ParticleSystem& particleSystem);
    vg::GBuffer getOrCreateFramebufferForParticleSystem(const nString& name);
    void renderPostProcess(const ParticleSystemData& particleSystemData, vg::GBuffer& gBuffer);

    ResourceManager& mResourceManager;
    const MaterialRenderer& mMaterialRenderer;

    // For use in multipass
    std::map<nString, vg::GBuffer> mGBuffers;
    vg::FullQuadVBO mFullQuadVbo;
    f32v2 mGbufferDims;
};

