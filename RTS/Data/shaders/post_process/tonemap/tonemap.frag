#include "GlobalUbo.glsl"

uniform sampler2D unLightTexture;

// Lighting uniforms (MaterialUtils::uploadTonemapUniforms)
uniform vec2 unGamma;
uniform vec2 unExposure;
uniform ivec2 unTonemapOperator;
uniform float unLightingSplit;

in vec2 fUV;

#include "util/tonemapping.glsl"

layout (location = 0) out vec3 oColor;

void main() {
    // Split view for light presets
    const int preset = int(step(unLightingSplit, fUV.x));


    vec3 pixelColor = texture(unLightTexture, fUV).rgb;
    pixelColor = computeTonemapping(pixelColor, unExposure[preset], unTonemapOperator[preset]);
   
    // Gamma correction
    oColor = pow(pixelColor, vec3(1.0 / unGamma[preset]));
}