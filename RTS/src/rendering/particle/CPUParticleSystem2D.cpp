#include "stdafx.h"
#include "CPUParticleSystem2D.h"

void CPUParticleSystem2D::updateAndRender() {
    for (auto&& it : mParticles) {
        for (CpuParticle2D& particle : it.second) {
            it.first(particle);
        }
    }
}

void CPUParticleSystem2D::addParticle(const ParticleUpdateFunction& function, f32v2 position, f32v2 velocity, f32v2 scale) {
    mParticles[function].push_back(CpuParticle2D{ position, velocity, scale, mParticleIdGen++ });
}
