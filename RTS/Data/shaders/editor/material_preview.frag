
#include "MaterialData.glsl"

uniform MaterialData unMaterialData;

in vec2 fUV;

layout (location = 0) out vec4 oColor;

void main() {
    vec2 adjustedUv = (fUV + 1.0) * 0.5;
    oColor = texture(sampler2D(unpackUint2x32(unMaterialData.albedoMap)), adjustedUv);
}