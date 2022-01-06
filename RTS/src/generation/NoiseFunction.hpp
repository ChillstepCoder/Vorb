#pragma once

#include "Noise.h"

class NoiseFunction {
public:
    NoiseFunction(const char* label, int octaves, f64 persistence, f64 frequency, f64v2 posOffset, f64 amplitude = 1.0, f64 heightOffset = 0.0) :
        label(label),
        octaves(octaves),
        persistence(persistence),
        frequency(frequency),
        posOffset(posOffset),
        amplitude(amplitude),
        heightOffset(heightOffset) { }

    virtual f64 compute(f64 x, f64 y) const {
        return (Noise::fractal(octaves, persistence, frequency, x + posOffset.x, y + posOffset.y) + heightOffset) * amplitude;
    }

    const char* label;
    int octaves = 5;
    f64 persistence = 0.7f;
    f64 frequency = 0.001f;
    f64v2 posOffset = f64v2(0.0);
    f64 amplitude = 1.0;
    f64 heightOffset = 0.0;
};

class RidgedNoiseFunction : public NoiseFunction {
public:
    RidgedNoiseFunction(const char* label, int octaves, f64 persistence, f64 frequency, f64v2 posOffset, f64 amplitude = 1.0, f64 heightOffset = 0.0) :
        NoiseFunction(label, octaves, persistence, frequency, posOffset, amplitude, heightOffset) {};

    f64 compute(f64 x, f64 y) const override {
        f64 h = Noise::fractal(octaves, persistence, frequency, x + posOffset.x, y + posOffset.y);
        h = 1.0 - abs(h);
        return (h + heightOffset) * amplitude;
    }
};