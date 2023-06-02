#pragma once

// TODO Make it so you can opt into the data you need for your particle.
// Each particle can have a component, or the component can be a single global component.
// You can also have a global color AND a per component color, and they will multiply (or custom render function?)
enum class ParticleComponentType {
    Position = BIT(0),
    Velocity = BIT(1),
    Scale = BIT(2),
    Color = BIT(3),
};

struct CpuParticle2D {
    f32v2 mPosition;
    f32v2 mVelocity;
    f32v2 mScale;
    ui32 mParticleId;
};

typedef std::function<void(CpuParticle2D& particle)> ParticleUpdateFunction;

class CPUParticleSystem2D
{
public:
    void updateAndRender();

    void addParticle(const ParticleUpdateFunction& function, f32v2 mosition, f32v2 velocity, f32v2 scale);

private:
    ui32 mParticleIdGen = 0;
    // TODO: Arrays of ParticleComponents?
    std::map<ParticleUpdateFunction, std::vector<CpuParticle2D>> mParticles;
};

