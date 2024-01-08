#pragma once

#include "math/Noise.h"

enum class NoiseFunctionType : ui8 {
    Standard,
    Ridged,
    Cellular,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(NoiseFunctionType,
    pair{ NoiseFunctionType::Standard, "std"sv },
    pair{ NoiseFunctionType::Ridged, "ridge"sv },
    pair{ NoiseFunctionType::Cellular, "cell"sv }
)

class NoiseFunction {
public:
    NoiseFunction() = default;
    NoiseFunction(StrToken label, NoiseFunctionType type, int octaves, f64 persistence, f64 frequency, f64v2 posOffset, f64 amplitude = 1.0, f64 heightOffset = 0.0) :
        label(label),
        type(type),
        octaves(octaves),
        persistence(persistence),
        frequency(frequency),
        posOffset(posOffset),
        amplitude(amplitude),
        heightOffset(heightOffset) { }
    ~NoiseFunction() = default;

    f64 compute(f64 x, f64 y) const {
        switch (type) {
            case NoiseFunctionType::Standard: {
                return (Noise::fractal(octaves, persistence, frequency, x + posOffset.x, y + posOffset.y) + heightOffset) * amplitude;
            }
            case NoiseFunctionType::Ridged: {
                f64 h = Noise::fractal(octaves, persistence, frequency, x + posOffset.x, y + posOffset.y);
                h = 1.0 - abs(h);
                return (h + heightOffset) * amplitude;
            }
            case NoiseFunctionType::Cellular: {
                f64v2 pos(x + posOffset.x, y + posOffset.y);
                f64 total = 0.0;
                f64 freq = frequency;
                f64 amp = 1.0;

                // We have to keep track of the largest possible amplitude,
                // because each octave adds more, and we need a value in [-1, 1].
                f64 maxAmplitude = 0.0;

                for (int i = 0; i < octaves; i++) {
                    f64v2 cval = Noise::cellularEuclidean(pos * freq);
                    total += (cval.y - cval.x) * amp;

                    freq *= 2.0;
                    maxAmplitude += amp;
                    amp *= persistence;
                }

                return (total / maxAmplitude) * amplitude;
                break;
            }
            default:
                break;

        }
        static_assert(e_count(NoiseFunctionType) == 3);
        return (Noise::fractal(octaves, persistence, frequency, x + posOffset.x, y + posOffset.y) + heightOffset) * amplitude;
    }

    f64v2 getRange() const {
        switch (type) {
            case NoiseFunctionType::Standard: {
                return f64v2(-1.0 + heightOffset, 1.0 + heightOffset) * amplitude;
            }
            case NoiseFunctionType::Ridged: {
                return f64v2(0.0 + heightOffset, 1.0 + heightOffset) * amplitude;
            }
            case NoiseFunctionType::Cellular: {
                return f64v2(-1.0 + heightOffset, 1.0 + heightOffset) * amplitude;
            }
            default:
                break;
        }
        static_assert(e_count(NoiseFunctionType) == 3);
        return f32v2(-1.0f, 1.0f) * (f32)amplitude;
    }

    StrToken label;
    NoiseFunctionType type = NoiseFunctionType::Standard;
    int octaves = 3;
    f64 persistence = 0.7f;
    f64 frequency = 0.001f;
    f64v2 posOffset = f64v2(0.0);
    f64 amplitude = 1.0;
    f64 heightOffset = 0.0;
};
SERIALIZABLE_IMGUI_CONTROLLED(NoiseFunction,
    make_field(o.label, "label"),
    make_field(o.type, "type"),
    make_field(o.octaves, "octaves"),
    make_field(o.persistence, "persistence"),
    make_field(o.frequency, "frequency"),
    make_field(o.posOffset, "posOffset"),
    make_field(o.amplitude, "amplitude"),
    make_field(o.heightOffset, "heightOffset")
)

// Optional noise function ptr
YML_WRITE_DEF_PTR(NoiseFunction);
YML_READ_DEF_PTR(NoiseFunction);