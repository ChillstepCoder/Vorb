#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in float fRoughness;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;

layout(std430, binding = 4) restrict readonly buffer BillboardMaterials {
	MaterialData inBillboardMaterials[];
};

void main() {
    MaterialData mtl = inBillboardMaterials[fMaterialIndex];
    vec4 color = vec4(1.0, 0.0, 0.0, 1.0); //sampleMaterialAlbedo(mtl, fUV) * fTint;
	
    //tryDiscardTransparentPixel(color.a);
    oColor.rgb = fTint.rgb;
	oColor.a = 1.0; // AO
	// Normal is always the next page
    vec3 normal = vec3(0.0);
    if (mtl.normalMap > 0) {
        vec3 normal = sampleMaterialNormal(mtl, fUV).rgb;
        normal = normal * 2.0 - 1.0;
    } else {
        normal = vec3(0.0, 0.0, 1.0);
    }
    normal = vec3(0.0, 0.0, 1.0); // TODO: REMOVE
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
    oMetallicRoughness.r = 0.0;
	oMetallicRoughness.g = fRoughness;
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(1.0, 0.0, 0.0);
}