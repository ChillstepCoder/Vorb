#include "MaterialData.glsl"

uniform sampler2D unTexture;

in vec2 fUV;
flat in uint fBufferIndex;

out vec4 fColor;

layout(std430, binding = 7) readonly buffer ParticleMaterial
{
    uint ParticleMaterials[];
};

void main() {
    MaterialData mtl = inMaterials[ParticleMaterials[fBufferIndex]];
    fColor = sampleMaterialAlbedo(mtl, fUV);
    if (fColor.a <= 0.01) {
        discard;
    }
}