
#include "MaterialData.glsl"

uniform MaterialData unMaterialData;

in vec2 fUV;

layout (location = 0) out vec4 oColor;

void main() {
    oColor = texture(sampler2D(unpackUint2x32(unMaterialData.albedoMap)), fUV);
}