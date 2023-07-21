#pragma once

class CPUParticleSystem;

class CpuParticleEmitter {
public:
    CpuParticleEmitter(CPUParticleSystem& system);
    ~CpuParticleEmitter();

protected:
    f32v2 mEmitRateRangeSec = f32v2(0.0f, 0.2f);
    TimePoint mLastEmittedParticleTime = TimePoint::min();
    CPUParticleSystem& mSystem;
};

