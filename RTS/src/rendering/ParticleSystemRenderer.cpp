#include "stdafx.h"
#include "ParticleSystemRenderer.h"

#include "ResourceManager.h"
#include "particles/ParticleSystem.h"
#include "particles/ParticleSystemManager.h"

ParticleSystemRenderer::ParticleSystemRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer) {

}

ParticleSystemRenderer::~ParticleSystemRenderer() {

}

void ParticleSystemRenderer::renderParticleSystems(const Camera2D& camera) {

    for (auto&& system : mResourceManager.getParticleSystemManager().mParticleSystems) {
        renderParticleSystem(camera, *system);
    }
}

void ParticleSystemRenderer::renderParticleSystem(const Camera2D& camera, const ParticleSystem& particleSystem) {
    // TODO:
    assert(false);
}
