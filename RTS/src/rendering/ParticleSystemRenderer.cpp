#include "stdafx.h"
#include "ParticleSystemRenderer.h"

#include "ResourceManager.h"
#include "particles/ParticleSystem.h"
#include "particles/ParticleSystemManager.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"

ParticleSystemRenderer::ParticleSystemRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer) {

}

ParticleSystemRenderer::~ParticleSystemRenderer() {

}

void ParticleSystemRenderer::renderLitParticleSystems(const Camera2D& camera) {

    for (auto&& system : mResourceManager.getParticleSystemManager().mParticleSystems) {
        if (!system->mSystemData.isEmissive) {
            renderParticleSystem(camera, *system);
        }
    }
}

void ParticleSystemRenderer::renderEmissiveParticleSystems(const Camera2D& camera) {

    for (auto&& system : mResourceManager.getParticleSystemManager().mParticleSystems) {
        if (system->mSystemData.isEmissive) {
            renderParticleSystem(camera, *system);
        }
    }
}

void ParticleSystemRenderer::renderParticleSystem(const Camera2D& camera, const ParticleSystem& particleSystem) {
    if (!particleSystem.mParticles.size()) {
        return;
    }

    const Material* material = mResourceManager.getMaterialManager().getMaterial(particleSystem.mSystemData.materialName);

    // Lazy mesh init
    if (!particleSystem.mVbo) {
        glGenVertexArrays(1, &particleSystem.mVao);
        glGenBuffers(1, &particleSystem.mVbo);
        glBindVertexArray(particleSystem.mVao);
        glBindBuffer(GL_ARRAY_BUFFER, particleSystem.mVbo);
        vg::GLProgram& program = material->mProgram;
        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(Particle), (void*)offsetof(Particle, mPosition));
        glVertexAttribPointer(program.getAttribute("vSize"), 1, GL_UNSIGNED_SHORT, false, sizeof(Particle), (void*)offsetof(Particle, mSize));

        if (const VGAttribute* attr = program.tryGetAttribute("vLifetime")) {
            glVertexAttribPointer(*attr, 1, GL_FLOAT, false, sizeof(Particle), (void*)offsetof(Particle, mLifeTime));
        }

        if (const VGAttribute* attr = program.tryGetAttribute("vVelocity")) {
            glVertexAttribPointer(program.getAttribute("vVelocity"), 3, GL_FLOAT, false, sizeof(Particle), (void*)offsetof(Particle, mVelocity));
        }
    }
    else {
        glBindVertexArray(particleSystem.mVao);
        glBindBuffer(GL_ARRAY_BUFFER, particleSystem.mVbo);
    }

    // Update mesh
    if (particleSystem.mDirty) {
        const int bufferSizeBytes = particleSystem.mParticles.size() * sizeof(Particle);
        // TODO: Look up buffer object streaming https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming 
        // Orphan the buffer for speed
        glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_STREAM_DRAW);
        // Set data
        glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, particleSystem.mParticles.data());
        particleSystem.mDirty = false;
    }

    assert(material);
    mMaterialRenderer.bindMaterialForRender(*material);

    glEnable(GL_PROGRAM_POINT_SIZE);
    material->mProgram.enableVertexAttribArrays();
    glDrawArrays(GL_POINTS, 0, particleSystem.mParticles.size());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
