#pragma once

namespace util {

    // Keep parity with wind.glsl
    extern f32v3 getModelWindOffset(f32v3 relativePosition, f32v3 modelRoot, int windType, f32 time);
}