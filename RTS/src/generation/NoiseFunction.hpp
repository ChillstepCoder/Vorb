#pragma once

struct NoiseFunction {

    inline f64 compute(f64 x, f64 y) const {
        return Noise::fractal(octaves, persistence, frequency, x, y);
    }

    int octaves = 5;
    f64 persistence = 0.7;
    f64 frequency = 0.001;
};