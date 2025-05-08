#include "stdafx.h"
#include "WindFormulas.h"

// THIS FILE MUST KEEP PARITY WITH wind.glsl


float hash(glm::vec2 p)  // replace this by something better
{
    p = 50.0f * glm::fract(p * 0.3183099f + glm::vec2(0.71f, 0.113f));
    return -1.0f + 2.0f * glm::fract(p.x * p.y * (p.x + p.y));
}

float noise(const glm::vec2& p) {
    glm::vec2 i = glm::floor(p);
    glm::vec2 f = glm::fract(p);

    glm::vec2 u = f * f * (glm::vec2(3.0f) - 2.0f * f);
    return glm::mix(
        glm::mix(hash(i + glm::vec2(0.0f, 0.0f)),
            hash(i + glm::vec2(1.0f, 0.0f)), u.x),
        glm::mix(hash(i + glm::vec2(0.0f, 1.0f)),
            hash(i + glm::vec2(1.0f, 1.0f)), u.x),
        u.y
    );
}

float fbm(glm::vec2 p) {
    glm::mat2 m(1.6f, 1.2f, -1.2f, 1.6f);
    float f = 0.0f;
    f += 0.5000f * noise(p); p = m * p;
    f += 0.2500f * noise(p); p = m * p;
    f += 0.1250f * noise(p); p = m * p;
    f += 0.0625f * noise(p);
    return f;
}

float norm(float f) {
    return 0.5f + 0.5f * f;
}

// Keep parity with wind.glsl
const float AMPLITUDE = 0.7f;
const float WIND_SPEED = 1.0f;
const float WIND_STRENGTH = 0.1f;
const float WORLD_SCALE = 0.2f;

// Keep parity with wind.glsl
float getWindAtPosition(float Time, f32v3 worldPos) {
    // Standard wind forces
    float windForce = fbm(f32v2(worldPos.x + Time * 0.075f, worldPos.y)) * AMPLITUDE;
    // Rolling wind
    windForce += std::sin((worldPos.x - worldPos.y) * 0.1f + Time * 0.2f) * 0.4f * AMPLITUDE;

    f32v3 scaledWorld = worldPos * WORLD_SCALE;
    f32v3 wind(
        std::sin(Time * WIND_SPEED + scaledWorld.x) + std::sin(Time * WIND_SPEED + scaledWorld.y * 2.0f) +
        std::sin(Time * WIND_SPEED * 0.1f + scaledWorld.x),
        std::cos(Time * WIND_SPEED + scaledWorld.x * 2.0f) + std::cos(Time * WIND_SPEED + scaledWorld.y),
        0.0f
    );
    windForce -= wind.x * WIND_STRENGTH + wind.y * WIND_STRENGTH;
    return windForce;
}

// Keep parity with wind.glsl
f32v3 util::getModelWindOffset(f32v3 relativePosition, f32v3 modelRoot, int windType, f32 time) {
    const float h = std::max(relativePosition.z, 0.001f);
    f32v3 trueWorldPosition = relativePosition + modelRoot;
    f32v3 offsetPosition = trueWorldPosition;
    if (windType == 1) { // Grass
        float windIntensity = getWindAtPosition(-time + h, offsetPosition) * 0.3f;
        windIntensity *= std::powf(h, 1.1f);
        glm::vec3 windOffset(windIntensity, windIntensity, 0.35f * windIntensity); // TODO: Follow bend like tree?
        offsetPosition += windOffset;
    }
    else if (windType == 2 || windType == 3) {
        // 2 == tree trunk
        // 3 == tree leaves
        float seed = time * 0.65f - (modelRoot.x - modelRoot.y) * 0.15f;
        float windIntensity = (std::sinf(seed * 0.5f) * h * h) * 0.005f;
        offsetPosition.x += windIntensity;
        float bendDown = std::abs(windIntensity);
        offsetPosition.z -= bendDown * bendDown * 0.1f; // Simulate bend (See wolfram alpha graph -(pow(abs(sin(x)), 2.0)) from 0 to 2PI)
        if (windType == 3) { // Tree Leaves jitter
            float jitter = fbm(f32v2(-offsetPosition.x + time * 0.175f, -offsetPosition.y)) * 0.2f;
            offsetPosition += f32v3(jitter, jitter, 0.3f * jitter);
        }
    }
    return offsetPosition - trueWorldPosition;
}
