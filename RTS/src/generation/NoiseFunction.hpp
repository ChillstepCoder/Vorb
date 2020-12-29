#pragma once

#include "Noise.h"

struct NoiseFunction {

    inline f64 compute(f64 x, f64 y) const {
        return Noise::fractal(octaves, persistence, frequency, x + offset.x, y + offset.y);
    }

    std::string label;
    int octaves = 5;
    f64 persistence = 0.7f;
    f64 frequency = 0.001f;
    f64v2 offset = f64v2(0.0);
};