#pragma once

class CPUParticleSystem2D;

class CpuParticleEmitter {
public:
    CpuParticleEmitter(CPUParticleSystem2D& system);
    ~CpuParticleEmitter();

protected:
    f32v2 mEmitRateRangeSec = f32v2(0.0f, 0.2f);
    TimePoint mLastEmittedParticleTime = TimePoint::min();
    CPUParticleSystem2D& mSystem;
};

