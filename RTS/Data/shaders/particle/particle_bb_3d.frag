#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fParticleMaterial;
flat in vec4 fColor;

out vec4 oColor;

void main() {
    MaterialData mtl = inMaterials[fParticleMaterial];
    oColor = sampleMaterialAlbedo(mtl, fUV) * fColor;
    //tryDiscardTransparentPixel(oColor.a);
}