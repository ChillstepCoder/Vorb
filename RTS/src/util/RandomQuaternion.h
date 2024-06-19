#pragma once

#include "math/Random.h"

namespace MathUtil {
    inline glm::quat randomQuaternion() {
        float u1 = Random::getCachedRandomf();
        float u2 = Random::getCachedRandomf();
        float u3 = Random::getCachedRandomf();

        float sqrt1MinusU1 = sqrtf(1.0f - u1);
        float sqrtU1 = sqrtf(u1);

        float theta1 = M_2_PIF * u2;
        float theta2 = M_2_PIF * u3;

        float x = sqrt1MinusU1 * sin(theta1);
        float y = sqrt1MinusU1 * cos(theta1);
        float z = sqrtU1 * sin(theta2);
        float w = sqrtU1 * cos(theta2);

        return glm::quat(w, x, y, z);
    }

    inline f32v3 randomUnitVector() {
        float theta = Random::getCachedRandomf() * 2.0f * glm::pi<float>(); // Random angle between 0 and 2*pi
        float phi = acos(2.0f * Random::getCachedRandomf() - 1.0f);         // Random angle between 0 and pi

        float x = sin(phi) * cos(theta);
        float y = sin(phi) * sin(theta);
        float z = cos(phi);

        return f32v3(x, y, z);
    }

    // Function to generate a random angular velocity vec3 with a given speed
    inline f32v3 randomAngularVelocity(f32 speed) {
        return randomUnitVector() * speed;
    }
}